#include "common.hh"
#include "judge_logger.hh"
#include "judge_or_rejudge_submission.hh"

#include <cstdint>
#include <optional>
#include <sim/contest_problems/contest_problem.hh>
#include <sim/internal_files/internal_file.hh>
#include <sim/jobs/job.hh>
#include <sim/judging_config.hh>
#include <sim/mysql/mysql.hh>
#include <sim/mysql/repeat_if_deadlocked.hh>
#include <sim/problems/problem.hh>
#include <sim/sql/sql.hh>
#include <sim/submissions/submission.hh>
#include <sim/submissions/update_final.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/escape_bytes_to_utf8_str.hh>
#include <simlib/macros/stack_unwinding.hh>
#include <simlib/sim/judge_worker.hh>
#include <simlib/string_transform.hh>
#include <simlib/time.hh>
#include <simlib/time_format_conversions.hh>

using sim::contest_problems::ContestProblem;
using sim::jobs::Job;
using sim::problems::Problem;
using sim::sql::Select;
using sim::sql::Update;
using sim::submissions::Submission;

namespace job_server::job_handlers {

void judge_or_rejudge_submission(
    sim::mysql::Connection& mysql,
    Logger& logger,
    decltype(Job::id) job_id,
    decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    decltype(Submission::file_id) submission_file_id;
    decltype(Submission::user_id) submission_user_id;
    decltype(Submission::problem_id) submission_problem_id;
    decltype(Problem::file_id) problem_file_id;
    decltype(Submission::contest_problem_id) submission_contest_problem_id;
    decltype(Submission::language) submission_language;
    decltype(Submission::last_judgment_began_at) submission_last_judgment_began_at;
    decltype(Job::created_at) job_created_at;

    auto transaction = mysql.start_repeatable_read_transaction();
    auto stmt =
        mysql.execute(Select("s.file_id, s.user_id, s.problem_id, p.file_id, s.contest_problem_id, "
                             "s.language, s.last_judgment_began_at, j.created_at")
                          .from("submissions s")
                          .inner_join("problems p")
                          .on("p.id=s.problem_id")
                          .inner_join("jobs j")
                          .on("j.id=?", job_id)
                          .where("s.id=?", submission_id));
    stmt.res_bind(
        submission_file_id,
        submission_user_id,
        submission_problem_id,
        problem_file_id,
        submission_contest_problem_id,
        submission_language,
        submission_last_judgment_began_at,
        job_created_at
    );
    if (!stmt.next()) {
        logger("Submission has been deleted.");
        mark_job_as_cancelled(mysql, logger, job_id);
        transaction.commit();
        return;
    }

    if (submission_last_judgment_began_at && *submission_last_judgment_began_at > job_created_at) {
        // Skip the job - the submission has already ben judged
        logger("Skipping judging of the submission because it has already ben judged after this "
               "job has been scheduled.");
        mark_job_as_cancelled(mysql, logger, job_id);
        transaction.commit();
        return;
    }

    auto current_judgment_began_at = utc_mysql_datetime();
    logger("Judging submission ", submission_id, " (problem: ", submission_problem_id, ')');
    sim::JudgeWorker judge_worker;
    logger("Loading problem package...");
    judge_worker.load_package(sim::internal_files::path_of(problem_file_id), std::nullopt);
    logger("... done.");

    auto update_submission = [&, submission_id](
                                 decltype(Submission::initial_status) initial_status,
                                 decltype(Submission::full_status) full_status,
                                 decltype(Submission::score) score,
                                 decltype(Submission::initial_report) initial_report,
                                 decltype(Submission::final_report) final_report
                             ) {
        // Submission type might have changed between transactions, get the actual type.
        decltype(Submission::type) submission_type;
        std::optional<decltype(ContestProblem::score_revealing)> contest_problem_score_revealing;
        auto stmt = mysql.execute(Select("s.type, cp.score_revealing")
                                      .from("submissions s")
                                      .left_join("contest_problems cp")
                                      .on("cp.id=s.contest_problem_id")
                                      .where("s.id=?", submission_id));
        stmt.res_bind(submission_type, contest_problem_score_revealing);
        if (!stmt.next()) {
            logger("Will not update submission because it disappeared.");
            return;
        }

        mysql.execute(
            Update("submissions")
                .set(
                    "initial_final_candidate=?, final_candidate=?, initial_status=?, "
                    "full_status=?, score=?, "
                    "initial_report=?, final_report=?",
                    sim::submissions::is_initial_final_candidate(
                        submission_type,
                        initial_status,
                        full_status,
                        score,
                        contest_problem_score_revealing
                    ),
                    sim::submissions::is_final_candidate(submission_type, full_status, score),
                    initial_status,
                    full_status,
                    score,
                    initial_report,
                    final_report
                )
                .where("id=?", submission_id)
        );
        sim::submissions::update_final(
            mysql, submission_user_id, submission_problem_id, submission_contest_problem_id
        );
    };

    logger("Compiling solution...");
    std::string compilation_errors;
    auto language = [submission_language] {
        // NOLINTNEXTLINE(bugprone-switch-missing-default-case)
        switch (submission_language) {
        case Submission::Language::C11: return sim::SolutionLanguage::C11;
        case Submission::Language::CPP11: return sim::SolutionLanguage::CPP11;
        case Submission::Language::CPP14: return sim::SolutionLanguage::CPP14;
        case Submission::Language::CPP17: return sim::SolutionLanguage::CPP17;
        case Submission::Language::CPP20: return sim::SolutionLanguage::CPP20;
        case Submission::Language::PASCAL: return sim::SolutionLanguage::PASCAL;
        case Submission::Language::PYTHON: return sim::SolutionLanguage::PYTHON;
        case Submission::Language::RUST: return sim::SolutionLanguage::RUST;
        }
        THROW("unexpected language");
    }();
    if (judge_worker.compile_solution(
            sim::internal_files::path_of(submission_file_id),
            language,
            sim::SOLUTION_COMPILATION_TIME_LIMIT,
            sim::SOLUTION_COMPILATION_MEMORY_LIMIT,
            &compilation_errors,
            sim::COMPILATION_ERRORS_MAX_LENGTH
        ))
    {
        logger("... failed:\n", compilation_errors);
        update_submission(
            Submission::Status::COMPILATION_ERROR,
            Submission::Status::COMPILATION_ERROR,
            std::nullopt,
            concat_tostr(
                "<pre class=\"compilation-errors\">", html_escape(compilation_errors), "</pre>"
            ),
            ""
        );
        mark_job_as_done(mysql, logger, job_id);
        transaction.commit();
        return;
    }
    logger("... done.");

    logger("Compiling checker...");
    compilation_errors = {};
    if (judge_worker.compile_checker(
            sim::CHECKER_COMPILATION_TIME_LIMIT,
            sim::CHECKER_COMPILATION_MEMORY_LIMIT,
            &compilation_errors,
            sim::COMPILATION_ERRORS_MAX_LENGTH
        ))
    {
        logger("... failed:\n", compilation_errors);
        update_submission(
            Submission::Status::CHECKER_COMPILATION_ERROR,
            Submission::Status::CHECKER_COMPILATION_ERROR,
            std::nullopt,
            "",
            ""
        );
        mark_job_as_done(mysql, logger, job_id);
        transaction.commit();
        return;
    }
    logger("... done.");
    update_job_log(mysql, logger, job_id);
    transaction.commit();

    auto construct_submission_judge_report = [](const sim::JudgeReport& judge_report) {
        std::string report;
        if (judge_report.groups.empty()) {
            return report;
        }

        // clang-format off
        back_insert(report, "<table class=\"table\">"
                          "<thead>"
                              "<tr>"
                                  "<th class=\"test\">Test</th>"
                                  "<th class=\"result\">Result</th>"
                                  "<th class=\"time\">Time [s]</th>"
                                  "<th class=\"memory\">Memory [KiB]</th>"
                                  "<th class=\"points\">Score</th>"
                              "</tr>"
                          "</thead>"
                          "<tbody>");
        // clang-format on
        bool there_are_comments = false;
        for (const auto& group : judge_report.groups) {
            bool first = true;
            for (const auto& test : group.tests) {
                back_insert(report, "<tr><td>", html_escape(test.name), "</td>");
                auto status_td = [&] {
                    switch (test.status) {
                    case sim::JudgeReport::Test::OK: return "<td class=\"status green\">OK</td>";
                    case sim::JudgeReport::Test::WA:
                        return "<td class=\"status red\">Wrong answer</td>";
                    case sim::JudgeReport::Test::TLE:
                        return "<td class=\"status yellow\">Time limit exceeded</td>";
                    case sim::JudgeReport::Test::MLE:
                        return "<td class=\"status yellow\">Memory limit exceeded</td>";
                    case sim::JudgeReport::Test::OLE:
                        return "<td class=\"status yellow\">Output size limit exceeded</td>";
                    case sim::JudgeReport::Test::RTE:
                        return "<td class=\"status intense-red\">Runtime error</td>";
                    case sim::JudgeReport::Test::CHECKER_ERROR:
                        return "<td class=\"status blue\">Checker error</td>";
                    case sim::JudgeReport::Test::SKIPPED:
                        return "<td class=\"status\">Pending</td>";
                    }
                    THROW("invalid test.status");
                }();
                back_insert(report, status_td, "<td>");
                // Time limit
                if (test.status == sim::JudgeReport::Test::SKIPPED) {
                    back_insert(report, '?');
                } else {
                    back_insert(report, to_string(floor_to_10ms(test.runtime), false));
                }
                back_insert(
                    report, " / ", to_string(floor_to_10ms(test.time_limit), false), "</td><td>"
                );
                // Memory limit
                if (test.status == sim::JudgeReport::Test::SKIPPED) {
                    back_insert(report, '?');
                } else {
                    back_insert(report, test.memory_consumed >> 10);
                }
                back_insert(report, " / ", test.memory_limit >> 10, "</td>");
                // Group score
                if (first) {
                    first = false;
                    back_insert(
                        report,
                        R"(<td class="groupscore" rowspan=")",
                        group.tests.size(),
                        "\">",
                        group.score,
                        " / ",
                        group.max_score,
                        "</td>"
                    );
                }
                back_insert(report, "</tr>");

                if (!test.comment.empty()) {
                    there_are_comments = true;
                }
            }
        }
        back_insert(report, "</tbody></table>");
        // Tests' comments
        if (there_are_comments) {
            back_insert(report, "<ul class=\"tests-comments\">");
            for (const auto& group : judge_report.groups) {
                for (const auto& test : group.tests) {
                    if (!test.comment.empty()) {
                        back_insert(
                            report,
                            "<li><span class=\"test-id\">",
                            html_escape(test.name),
                            "</span>",
                            html_escape(escape_bytes_to_utf8_str("", test.comment, "")),
                            "</li>"
                        );
                    }
                }
            }
            back_insert(report, "</ul>");
        }
        return report;
    };

    auto calculate_submission_status = [](const sim::JudgeReport& judge_report) {
        // Check for judge errors
        for (const auto& group : judge_report.groups) {
            for (const auto& test : group.tests) {
                if (test.status == sim::JudgeReport::Test::CHECKER_ERROR) {
                    return Submission::Status::JUDGE_ERROR;
                }
            }
        }

        for (const auto& group : judge_report.groups) {
            for (const auto& test : group.tests) {
                switch (test.status) {
                case sim::JudgeReport::Test::OK:
                case sim::JudgeReport::Test::SKIPPED: continue;
                case sim::JudgeReport::Test::WA: return Submission::Status::WA;
                case sim::JudgeReport::Test::TLE: return Submission::Status::TLE;
                case sim::JudgeReport::Test::MLE: return Submission::Status::MLE;
                case sim::JudgeReport::Test::OLE: return Submission::Status::OLE;
                case sim::JudgeReport::Test::RTE: return Submission::Status::RTE;
                case sim::JudgeReport::Test::CHECKER_ERROR:
                    THROW("BUG: this should be handled above");
                }
            }
        }
        return Submission::Status::OK;
    };

    std::optional<std::string> initial_report;
    auto process_judge_report = [&,
                                 initial_report = std::optional<std::string>{},
                                 initial_score = int64_t{0},
                                 initial_status = Submission::Status::PENDING](
                                    const sim::JudgeReport& judge_report, bool final, bool partial
                                ) mutable {
        auto report = construct_submission_judge_report(judge_report);
        auto status = calculate_submission_status(judge_report);
        // Count score
        int64_t score = 0;
        for (const auto& group : judge_report.groups) {
            score += group.score;
        }

        sim::mysql::repeat_if_deadlocked(128, [&] {
            auto transaction = mysql.start_repeatable_read_transaction();
            if (!final) {
                initial_report = report;
                initial_score = score;
                initial_status = status;
                update_submission(
                    initial_status,
                    (initial_status != Submission::Status::OK ? initial_status
                                                              : Submission::Status::PENDING),
                    std::nullopt,
                    std::move(report),
                    ""
                );
                update_job_log(mysql, logger, job_id);
                transaction.commit();
                return;
            }
            // Final
            score += initial_score;
            if (initial_status != Submission::Status::OK &&
                status != Submission::Status::JUDGE_ERROR)
            {
                status = initial_status;
            }
            update_submission(
                initial_status, status, score, initial_report.value_or(""), std::move(report)
            );
            if (partial) {
                update_job_log(mysql, logger, job_id);
            } else {
                mark_job_as_done(mysql, logger, job_id);
            }
            transaction.commit();
        });
    };

    JudgeLogger judge_logger(logger);
    auto initial_judge_report =
        judge_worker.judge(false, judge_logger, [&](const sim::JudgeReport& partial_judge_report) {
            process_judge_report(partial_judge_report, false, true);
        });
    process_judge_report(initial_judge_report, false, false);

    auto final_judge_report =
        judge_worker.judge(true, judge_logger, [&](const sim::JudgeReport& partial_judge_report) {
            process_judge_report(partial_judge_report, true, true);
        });
    process_judge_report(final_judge_report, true, false);
}
} // namespace job_server::job_handlers

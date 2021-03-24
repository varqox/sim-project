#pragma once

#include "src/sim_merger/contest_problems.hh"
#include "src/sim_merger/internal_files.hh"
#include "src/sim_merger/problems.hh"

namespace sim_merger {

class SubmissionsMerger : public Merger<sim::submissions::Submission> {
    const InternalFilesMerger& internal_files_;
    const UsersMerger& users_;
    const ProblemsMerger& problems_;
    const ContestProblemsMerger& contest_problems_;
    const ContestRoundsMerger& contest_rounds_;
    const ContestsMerger& contests_;

    void load(RecordSet& record_set) override {
        STACK_UNWINDING_MARK;

        sim::submissions::Submission s;
        mysql::Optional<decltype(s.owner)::value_type> m_owner;
        mysql::Optional<decltype(s.contest_problem_id)::value_type> m_contest_problem_id;
        mysql::Optional<decltype(s.contest_round_id)::value_type> m_contest_round_id;
        mysql::Optional<decltype(s.contest_id)::value_type> m_contest_id;
        mysql::Optional<decltype(s.score)::value_type> m_score;
        auto stmt = conn.prepare(
            "SELECT id, file_id, owner, problem_id,"
            " contest_problem_id, contest_round_id, contest_id,"
            " type, language, final_candidate, problem_final,"
            " contest_final, contest_initial_final, initial_status,"
            " full_status, submit_time, score, last_judgment,"
            " initial_report, final_report "
            "FROM ",
            record_set.sql_table_name);
        stmt.bind_and_execute();
        stmt.res_bind_all(
            s.id, s.file_id, m_owner, s.problem_id, m_contest_problem_id, m_contest_round_id,
            m_contest_id, s.type, s.language, s.final_candidate, s.problem_final,
            s.contest_final, s.contest_initial_final, s.initial_status, s.full_status,
            s.submit_time, m_score, s.last_judgment, s.initial_report, s.final_report);
        while (stmt.next()) {
            s.owner = m_owner.to_opt();
            s.contest_problem_id = m_contest_problem_id.to_opt();
            s.contest_round_id = m_contest_round_id.to_opt();
            s.contest_id = m_contest_id.to_opt();
            s.score = m_score.to_opt();

            s.file_id = internal_files_.new_id(s.file_id, record_set.kind);
            if (s.owner) {
                s.owner = users_.new_id(s.owner.value(), record_set.kind);
            }
            s.problem_id = problems_.new_id(s.problem_id, record_set.kind);
            if (s.contest_problem_id) {
                s.contest_problem_id =
                    contest_problems_.new_id(s.contest_problem_id.value(), record_set.kind);
            }
            if (s.contest_round_id) {
                s.contest_round_id =
                    contest_rounds_.new_id(s.contest_round_id.value(), record_set.kind);
            }
            if (s.contest_id) {
                s.contest_id = contests_.new_id(s.contest_id.value(), record_set.kind);
            }

            record_set.add_record(
                s,
                str_to_time_point(intentional_unsafe_cstring_view(s.submit_time.to_string())));
        }
    }

    void merge() override {
        STACK_UNWINDING_MARK;
        Merger::merge([&](const sim::submissions::Submission& /*unused*/) { return nullptr; });
    }

public:
    void save_merged() override {
        STACK_UNWINDING_MARK;
        auto transaction = conn.start_transaction();
        conn.update("TRUNCATE ", sql_table_name());
        auto stmt = conn.prepare(
            "INSERT INTO ", sql_table_name(),
            "(id, file_id, owner, problem_id, contest_problem_id,"
            " contest_round_id, contest_id, type, language,"
            " final_candidate, problem_final, contest_final,"
            " contest_initial_final, initial_status, full_status,"
            " submit_time, score, last_judgment, initial_report,"
            " final_report) "
            "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?,"
            " ?, ?, ?, ?)");

        ProgressBar progress_bar("Submissions saved:", new_table_.size(), 128);
        for (const NewRecord& new_record : new_table_) {
            Defer progressor = [&] { progress_bar.iter(); };
            const auto& x = new_record.data;
            stmt.bind_and_execute(
                x.id, x.file_id, x.owner, x.problem_id, x.contest_problem_id,
                x.contest_round_id, x.contest_id, x.type, x.language, x.final_candidate,
                x.problem_final, x.contest_final, x.contest_initial_final, x.initial_status,
                x.full_status, x.submit_time, x.score, x.last_judgment, x.initial_report,
                x.final_report);
        }

        conn.update("ALTER TABLE ", sql_table_name(), " AUTO_INCREMENT=", last_new_id_ + 1);
        transaction.commit();
    }

    SubmissionsMerger(
        const IdsFromMainAndOtherJobs& ids_from_both_jobs,
        const InternalFilesMerger& internal_files, const UsersMerger& users,
        const ProblemsMerger& problems, const ContestProblemsMerger& contest_problems,
        const ContestRoundsMerger& contest_rounds, const ContestsMerger& contests)
    : Merger(
          "submissions", ids_from_both_jobs.main.submissions,
          ids_from_both_jobs.other.submissions)
    , internal_files_(internal_files)
    , users_(users)
    , problems_(problems)
    , contest_problems_(contest_problems)
    , contest_rounds_(contest_rounds)
    , contests_(contests) {
        STACK_UNWINDING_MARK;
        initialize();
    }
};

} // namespace sim_merger

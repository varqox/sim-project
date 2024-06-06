#pragma once

#include "sim_merger.hh"

#include <chrono>
#include <map>
#include <sim/contest_entry_tokens/old_contest_entry_token.hh>
#include <sim/contest_files/old_contest_file.hh>
#include <sim/contest_problems/old_contest_problem.hh>
#include <sim/contest_rounds/old_contest_round.hh>
#include <sim/contest_users/old_contest_user.hh>
#include <sim/contests/old_contest.hh>
#include <sim/internal_files/old_internal_file.hh>
#include <sim/jobs/old_job.hh>
#include <sim/jobs/utils.hh>
#include <sim/old_sql_fields/blob.hh>
#include <sim/old_sql_fields/datetime.hh>
#include <sim/problem_tags/old_problem_tag.hh>
#include <sim/problems/old_problem.hh>
#include <sim/sessions/old_session.hh>
#include <sim/submissions/old_submission.hh>
#include <sim/users/old_user.hh>
#include <simlib/from_unsafe.hh>
#include <simlib/ranges.hh>
#include <simlib/time.hh>
#include <simlib/time_format_conversions.hh>

namespace sim_merger {

// For each id, we keep the first date that it appeared with
template <class IdType>
struct PrimaryKeysWithTime {
    std::map<IdType, std::chrono::system_clock::time_point> ids;

    void add_id(IdType id, std::chrono::system_clock::time_point tp) {
        auto it = ids.find(id);
        if (it == ids.end()) {
            ids.emplace(id, tp);
            return;
        }

        if (it->second > tp) {
            it->second = tp;
        }
    }

    void normalize_times() {
        if (ids.empty()) {
            return;
        }

        auto last_tp = (--ids.end())->second;
        for (auto& [id, tp] : reverse_view(ids)) {
            if (tp > last_tp) {
                tp = last_tp;
            } else {
                last_tp = tp;
            }
        }
    }
};

// Loads all information about ids from jobs
struct PrimaryKeysFromJobs {
    PrimaryKeysWithTime<decltype(sim::internal_files::OldInternalFile::primary_key)::Type>
        internal_files;
    PrimaryKeysWithTime<decltype(sim::users::OldUser::primary_key)::Type> users;
    PrimaryKeysWithTime<decltype(sim::sessions::OldSession::primary_key)::Type> sessions;
    PrimaryKeysWithTime<decltype(sim::problems::OldProblem::primary_key)::Type> problems;
    PrimaryKeysWithTime<decltype(sim::problem_tags::OldProblemTag::primary_key)::Type> problem_tags;
    PrimaryKeysWithTime<decltype(sim::contests::OldContest::primary_key)::Type> contests;
    PrimaryKeysWithTime<decltype(sim::contest_rounds::OldContestRound::primary_key)::Type>
        contest_rounds;
    PrimaryKeysWithTime<decltype(sim::contest_problems::OldContestProblem::primary_key)::Type>
        contest_problems;
    PrimaryKeysWithTime<decltype(sim::contest_users::OldContestUser::primary_key)::Type>
        contest_users;
    PrimaryKeysWithTime<decltype(sim::contest_files::OldContestFile::primary_key)::Type>
        contest_files;
    PrimaryKeysWithTime<decltype(sim::contest_entry_tokens::OldContestEntryToken::primary_key
    )::Type>
        contest_entry_tokens;
    PrimaryKeysWithTime<decltype(sim::submissions::OldSubmission::primary_key)::Type> submissions;
    PrimaryKeysWithTime<decltype(sim::jobs::OldJob::primary_key)::Type> jobs;

    void initialize(StringView job_table_name) {
        STACK_UNWINDING_MARK;
        using sim::jobs::OldJob;

        decltype(OldJob::id) id = 0;
        old_mysql::Optional<decltype(OldJob::creator)::value_type> creator;
        EnumVal<OldJob::Type> type{};
        old_mysql::Optional<decltype(OldJob::file_id)::value_type> file_id;
        sim::old_sql_fields::Datetime added_str;
        old_mysql::Optional<decltype(OldJob::aux_id)::value_type> aux_id;
        old_mysql::Optional<decltype(OldJob::aux_id)::value_type> aux_id_2;
        sim::old_sql_fields::Blob<32> info;
        auto old_mysql = old_mysql::ConnectionView{*mysql};
        auto stmt = old_mysql.prepare(
            "SELECT id, creator, type, file_id, "
            "created_at, aux_id, aux_id_2, info FROM ",
            job_table_name,
            " ORDER BY id"
        );
        stmt.bind_and_execute();
        stmt.res_bind_all(id, creator, type, file_id, added_str, aux_id, aux_id_2, info);
        while (stmt.next()) {
            auto created_at = str_to_time_point(added_str.to_cstr());

            // Process non-type-specific ids
            jobs.add_id(id, created_at);
            if (file_id.has_value()) {
                internal_files.add_id(file_id.value(), created_at);
            }
            if (creator.has_value()) {
                users.add_id(creator.value(), created_at);
            }

            // Process type-specific ids
            switch (type) {
            case OldJob::Type::DELETE_FILE:
                // Id is already processed
                break;

            case OldJob::Type::JUDGE_SUBMISSION:
            case OldJob::Type::REJUDGE_SUBMISSION:
                submissions.add_id(aux_id.value(), created_at);
                break;

            case OldJob::Type::DELETE_PROBLEM:
            case OldJob::Type::REUPLOAD_PROBLEM:
            case OldJob::Type::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
            case OldJob::Type::CHANGE_PROBLEM_STATEMENT:
                problems.add_id(aux_id.value(), created_at);
                break;

            case OldJob::Type::MERGE_PROBLEMS:
                problems.add_id(aux_id.value(), created_at);
                problems.add_id(aux_id_2.value(), created_at);
                break;

            case OldJob::Type::DELETE_USER: users.add_id(aux_id.value(), created_at); break;

            case OldJob::Type::MERGE_USERS:
                users.add_id(aux_id.value(), created_at);
                users.add_id(aux_id_2.value(), created_at);
                break;

            case OldJob::Type::DELETE_CONTEST: contests.add_id(aux_id.value(), created_at); break;

            case OldJob::Type::DELETE_CONTEST_ROUND:
                contest_rounds.add_id(aux_id.value(), created_at);
                break;

            case OldJob::Type::DELETE_CONTEST_PROBLEM:
            case OldJob::Type::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
                contest_problems.add_id(aux_id.value(), created_at);
                break;

            case OldJob::Type::ADD_PROBLEM:
                if (aux_id.has_value()) {
                    problems.add_id(aux_id.value(), created_at);
                }
                break;

            case OldJob::Type::EDIT_PROBLEM: THROW("TODO");
            }
        }
    }

    explicit PrimaryKeysFromJobs(StringView job_table_name) {
        STACK_UNWINDING_MARK;
        initialize(job_table_name);
    }
};

struct PrimaryKeysFromMainAndOtherJobs {
    PrimaryKeysFromJobs main{from_unsafe{concat(main_sim_table_prefix, "jobs")}};
    PrimaryKeysFromJobs other{"jobs"};
};

} // namespace sim_merger

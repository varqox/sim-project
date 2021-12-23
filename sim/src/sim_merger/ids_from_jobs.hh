#pragma once

#include "sim/contest_entry_tokens/contest_entry_token.hh"
#include "sim/contest_files/contest_file.hh"
#include "sim/contest_problems/contest_problem.hh"
#include "sim/contest_rounds/contest_round.hh"
#include "sim/contest_users/contest_user.hh"
#include "sim/contests/contest.hh"
#include "sim/internal_files/internal_file.hh"
#include "sim/jobs/job.hh"
#include "sim/jobs/utils.hh"
#include "sim/problem_tags/problem_tag.hh"
#include "sim/problems/problem.hh"
#include "sim/sessions/session.hh"
#include "sim/sql_fields/blob.hh"
#include "sim/sql_fields/datetime.hh"
#include "sim/submissions/submission.hh"
#include "sim/users/user.hh"
#include "simlib/ranges.hh"
#include "simlib/time.hh"
#include "src/sim_merger/sim_merger.hh"

#include <chrono>
#include <map>

namespace sim_merger {

// For each id, we keep the first date that it appeared with
template <class IdType, class IdCmp = std::less<IdType>>
struct IdsWithTime {
    std::map<IdType, std::chrono::system_clock::time_point, IdCmp> ids;

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

struct ProblemTagIdCmp {
    bool operator()(const decltype(sim::problem_tags::ProblemTag::id)& a,
            const decltype(sim::problem_tags::ProblemTag::id)& b) const noexcept {
        return std::pair{a.problem_id, a.tag} < std::pair{b.problem_id, b.tag};
    }
};

struct ContestUserIdCmp {
    bool operator()(const decltype(sim::contest_users::ContestUser::id)& a,
            const decltype(sim::contest_users::ContestUser::id)& b) const noexcept {
        return std::pair{a.user_id, a.contest_id} < std::pair{b.user_id, b.contest_id};
    }
};

// Loads all information about ids from jobs
struct IdsFromJobs {
    IdsWithTime<decltype(sim::internal_files::InternalFile::id)> internal_files;
    IdsWithTime<decltype(sim::users::User::id)> users;
    IdsWithTime<decltype(sim::sessions::Session::id)> sessions;
    IdsWithTime<decltype(sim::problems::Problem::id)> problems;
    IdsWithTime<decltype(sim::problem_tags::ProblemTag::id), ProblemTagIdCmp> problem_tags;
    IdsWithTime<decltype(sim::contests::Contest::id)> contests;
    IdsWithTime<decltype(sim::contest_rounds::ContestRound::id)> contest_rounds;
    IdsWithTime<decltype(sim::contest_problems::ContestProblem::id)> contest_problems;
    IdsWithTime<decltype(sim::contest_users::ContestUser::id), ContestUserIdCmp> contest_users;
    IdsWithTime<decltype(sim::contest_files::ContestFile::id)> contest_files;
    IdsWithTime<decltype(sim::contest_entry_tokens::ContestEntryToken::token)>
            contest_entry_tokens;
    IdsWithTime<decltype(sim::submissions::Submission::id)> submissions;
    IdsWithTime<decltype(sim::jobs::Job::id)> jobs;

    void initialize(StringView job_table_name) {
        STACK_UNWINDING_MARK;
        using sim::jobs::Job;

        decltype(Job::id) id = 0;
        mysql::Optional<decltype(Job::creator)::value_type> creator;
        EnumVal<Job::Type> type{};
        mysql::Optional<decltype(Job::file_id)::value_type> file_id;
        mysql::Optional<decltype(Job::tmp_file_id)::value_type> tmp_file_id;
        sim::sql_fields::Datetime added_str;
        mysql::Optional<decltype(Job::aux_id)::value_type> aux_id;
        sim::sql_fields::Blob<32> info;

        auto stmt = conn.prepare("SELECT id, creator, type, file_id, "
                                 "tmp_file_id, added, aux_id, info FROM ",
                job_table_name, " ORDER BY id");
        stmt.bind_and_execute();
        stmt.res_bind_all(id, creator, type, file_id, tmp_file_id, added_str, aux_id, info);
        while (stmt.next()) {
            auto added = str_to_time_point(added_str.to_cstr());

            // Process non-type-specific ids
            jobs.add_id(id, added);
            if (file_id.has_value()) {
                internal_files.add_id(file_id.value(), added);
            }
            if (tmp_file_id.has_value()) {
                internal_files.add_id(tmp_file_id.value(), added);
            }
            if (creator.has_value()) {
                users.add_id(creator.value(), added);
            }

            // Process type-specific ids
            switch (type) {
            case Job::Type::DELETE_FILE:
                // Id is already processed
                break;

            case Job::Type::JUDGE_SUBMISSION:
            case Job::Type::REJUDGE_SUBMISSION:
                submissions.add_id(aux_id.value(), added);
                break;

            case Job::Type::DELETE_PROBLEM:
            case Job::Type::REUPLOAD_PROBLEM:
            case Job::Type::REUPLOAD_PROBLEM__JUDGE_MODEL_SOLUTION:
            case Job::Type::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
            case Job::Type::CHANGE_PROBLEM_STATEMENT:
                problems.add_id(aux_id.value(), added);
                break;

            case Job::Type::MERGE_PROBLEMS:
                problems.add_id(aux_id.value(), added);
                problems.add_id(sim::jobs::MergeProblemsInfo(info).target_problem_id, added);
                break;

            case Job::Type::DELETE_USER: users.add_id(aux_id.value(), added); break;

            case Job::Type::MERGE_USERS:
                users.add_id(aux_id.value(), added);
                users.add_id(sim::jobs::MergeUsersInfo(info).target_user_id, added);
                break;

            case Job::Type::DELETE_CONTEST: contests.add_id(aux_id.value(), added); break;

            case Job::Type::DELETE_CONTEST_ROUND:
                contest_rounds.add_id(aux_id.value(), added);
                break;

            case Job::Type::DELETE_CONTEST_PROBLEM:
            case Job::Type::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
                contest_problems.add_id(aux_id.value(), added);
                break;

            case Job::Type::ADD_PROBLEM:
            case Job::Type::ADD_PROBLEM__JUDGE_MODEL_SOLUTION:
                if (aux_id.has_value()) {
                    problems.add_id(aux_id.value(), added);
                }
                break;

            case Job::Type::EDIT_PROBLEM: THROW("TODO");
            }
        }
    }

    explicit IdsFromJobs(StringView job_table_name) {
        STACK_UNWINDING_MARK;
        initialize(job_table_name);
    }
};

struct IdsFromMainAndOtherJobs {
    IdsFromJobs main{intentional_unsafe_string_view(concat(main_sim_table_prefix, "jobs"))};
    IdsFromJobs other{"jobs"};
};

} // namespace sim_merger

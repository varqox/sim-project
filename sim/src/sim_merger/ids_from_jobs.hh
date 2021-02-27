#pragma once

#include "sim/constants.hh"
#include "sim/contest_problems/contest_problem.hh"
#include "sim/contest_rounds/contest_round.hh"
#include "sim/contest_users/contest_user.hh"
#include "sim/contests/contest.hh"
#include "sim/jobs/jobs.hh"
#include "sim/problems/problem.hh"
#include "sim/sessions/session.hh"
#include "sim/sql_fields/blob.hh"
#include "sim/sql_fields/datetime.hh"
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

struct ProblemTagId {
    uintmax_t problem_id;
    InplaceBuff<sim::PROBLEM_TAG_MAX_LEN> tag;
};

inline bool operator<(const ProblemTagId& a, const ProblemTagId& b) noexcept {
    return std::pair(a.problem_id, a.tag) < std::pair(b.problem_id, b.tag);
}

inline auto stringify(ProblemTagId ptag) {
    return concat("(problem: ", ptag.problem_id, " tag: ", ptag.tag, ')');
}

struct ContestUserIdCmp {
    bool operator()(
        const decltype(sim::contest_users::ContestUser::id)& a,
        const decltype(sim::contest_users::ContestUser::id)& b) const noexcept {
        return std::pair(a.user_id, a.contest_id) < std::pair(b.user_id, b.contest_id);
    }
};

// Loads all information about ids from jobs
struct IdsFromJobs {
    IdsWithTime<uintmax_t> internal_files; // TODO: replace with appropriate decltype
    IdsWithTime<decltype(sim::users::User::id)> users;
    IdsWithTime<decltype(sim::sessions::Session::id)> sessions;
    IdsWithTime<decltype(sim::problems::Problem::id)> problems;
    IdsWithTime<ProblemTagId> problem_tags; // TODO: replace with appropriate decltype
    IdsWithTime<decltype(sim::contests::Contest::id)> contests;
    IdsWithTime<decltype(sim::contest_rounds::ContestRound::id)> contest_rounds;
    IdsWithTime<decltype(sim::contest_problems::ContestProblem::id)> contest_problems;
    IdsWithTime<decltype(sim::contest_users::ContestUser::id), ContestUserIdCmp> contest_users;
    IdsWithTime<InplaceBuff<::sim::FILE_ID_LEN>>
        contest_files; // TODO: replace with appropriate decltype
    IdsWithTime<InplaceBuff<::sim::CONTEST_ENTRY_TOKEN_LEN>>
        contest_entry_tokens; // TODO: replace with appropriate decltype
    IdsWithTime<uintmax_t> submissions; // TODO: replace with appropriate decltype
    IdsWithTime<uintmax_t> jobs; // TODO: replace with appropriate decltype

    void initialize(StringView job_table_name) {
        STACK_UNWINDING_MARK;
        using sim::JobType;

        uintmax_t id = 0;
        mysql::Optional<uintmax_t> creator;
        EnumVal<JobType> type{};
        mysql::Optional<uintmax_t> file_id;
        mysql::Optional<uintmax_t> tmp_file_id;
        sim::sql_fields::Datetime added_str;
        mysql::Optional<uintmax_t> aux_id;
        sim::sql_fields::Blob<32> info;

        auto stmt = conn.prepare(
            "SELECT id, creator, type, file_id, "
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
            case JobType::DELETE_FILE:
                // Id is already processed
                break;

            case JobType::JUDGE_SUBMISSION:
            case JobType::REJUDGE_SUBMISSION: submissions.add_id(aux_id.value(), added); break;

            case JobType::DELETE_PROBLEM:
            case JobType::REUPLOAD_PROBLEM:
            case JobType::REUPLOAD_PROBLEM__JUDGE_MODEL_SOLUTION:
            case JobType::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
            case JobType::CHANGE_PROBLEM_STATEMENT:
                problems.add_id(aux_id.value(), added);
                break;

            case JobType::MERGE_PROBLEMS:
                problems.add_id(aux_id.value(), added);
                problems.add_id(sim::jobs::MergeProblemsInfo(info).target_problem_id, added);
                break;

            case JobType::DELETE_USER: users.add_id(aux_id.value(), added); break;

            case JobType::MERGE_USERS:
                users.add_id(aux_id.value(), added);
                users.add_id(sim::jobs::MergeUsersInfo(info).target_user_id, added);
                break;

            case JobType::DELETE_CONTEST: contests.add_id(aux_id.value(), added); break;

            case JobType::DELETE_CONTEST_ROUND:
                contest_rounds.add_id(aux_id.value(), added);
                break;

            case JobType::DELETE_CONTEST_PROBLEM:
            case JobType::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
                contest_problems.add_id(aux_id.value(), added);
                break;

            case JobType::ADD_PROBLEM:
            case JobType::ADD_PROBLEM__JUDGE_MODEL_SOLUTION:
                if (aux_id.has_value()) {
                    problems.add_id(aux_id.value(), added);
                }
                break;

            case JobType::EDIT_PROBLEM: THROW("TODO");
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

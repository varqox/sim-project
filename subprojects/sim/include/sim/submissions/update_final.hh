#pragma once

#include <sim/contest_problems/contest_problem.hh>
#include <sim/mysql/mysql.hh>
#include <sim/old_mysql/old_mysql.hh>
#include <sim/submissions/submission.hh>

namespace sim::submissions {

bool is_final_candidate(
    decltype(sim::submissions::Submission::type) type,
    decltype(sim::submissions::Submission::full_status) full_status,
    decltype(sim::submissions::Submission::score) score
);

bool is_initial_final_candidate(
    decltype(sim::submissions::Submission::type) type,
    decltype(sim::submissions::Submission::initial_status) initial_status,
    decltype(sim::submissions::Submission::full_status) full_status,
    decltype(sim::submissions::Submission::score) score,
    std::optional<sim::contest_problems::ContestProblem::ScoreRevealing>
        contest_problem_score_revealing
);

// Has to be called inside a transaction
void update_final(
    sim::mysql::Connection& mysql,
    decltype(sim::submissions::Submission::user_id) submission_user_id,
    decltype(sim::submissions::Submission::problem_id) submission_problem_id,
    decltype(sim::submissions::Submission::contest_problem_id) submission_contest_problem_id
);

} // namespace sim::submissions

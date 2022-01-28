#pragma once

#include "sim/problems/problem.hh"
#include "sim/users/user.hh"
#include "src/web_server/web_worker/context.hh"

namespace web_server::capabilities {

struct ProblemsCapabilities {
    bool web_ui_view : 1;
    bool add_with_type_private : 1;
    bool add_with_type_contest_only : 1;
    bool add_with_type_public : 1;
};

ProblemsCapabilities problems(const decltype(web_worker::Context::session)& session) noexcept;

bool list_all_problems(const decltype(web_worker::Context::session)& session) noexcept;

bool list_problems_by_type(const decltype(web_worker::Context::session)& session,
        decltype(sim::problems::Problem::type) problem_type) noexcept;

bool list_problems_of_user(const decltype(web_worker::Context::session)& session,
        decltype(sim::users::User::id) user_id) noexcept;

bool list_problems_of_user_by_type(const decltype(web_worker::Context::session)& session,
        decltype(sim::users::User::id) user_id,
        decltype(sim::problems::Problem::type) problem_type) noexcept;

struct ProblemCapabilities {
    bool view : 1;
    bool view_statement : 1;
    bool view_public_tags : 1;
    bool view_hidden_tags : 1;
    bool view_owner : 1;
    bool view_simfile : 1;
    bool download : 1;
    bool reupload : 1;
    bool rejudge_all_submissions : 1;
    bool reset_time_limits : 1;
    bool delete_ : 1;
    bool merge_into_another_problem : 1;
    bool merge_other_problem_into_this_problem : 1;
};

ProblemCapabilities problem(const decltype(web_server::web_worker::Context::session)& session,
        decltype(sim::problems::Problem::type) problem_type,
        decltype(sim::problems::Problem::owner_id) problem_owner_id) noexcept;

} // namespace web_server::capabilities

#include "src/web_server/capabilities/problems.hh"
#include "sim/problems/problem.hh"
#include "sim/users/user.hh"
#include "src/web_server/capabilities/utils.hh"
#include "src/web_server/web_worker/context.hh"

using sim::problems::Problem;
using sim::users::User;
using web_server::web_worker::Context;

namespace web_server::capabilities {

ProblemsCapabilities problems(const decltype(Context::session)& session) noexcept {
    return ProblemsCapabilities{
            .web_ui_view = true,
            .add_with_type_private = is_admin(session) or is_teacher(session),
            .add_with_type_contest_only = is_admin(session) or is_teacher(session),
            .add_with_type_public = is_admin(session),
    };
}

bool list_all_problems(const decltype(Context::session)& session) noexcept {
    return is_admin(session);
}

bool list_problems_by_type(const decltype(Context::session)& session,
        decltype(Problem::type) problem_type) noexcept {
    switch (problem_type) {
    case Problem::Type::PRIVATE: return is_admin(session);
    case Problem::Type::CONTEST_ONLY: return is_admin(session) or is_teacher(session);
    case Problem::Type::PUBLIC: return true;
    }
}

bool list_problems_of_user(
        const decltype(Context::session)& session, decltype(User::id) user_id) noexcept {
    return is_self(session, user_id) or is_admin(session);
}

bool list_problems_of_user_by_type(const decltype(Context::session)& session,
        decltype(User::id) user_id, decltype(Problem::type) /*problem_type*/) noexcept {
    return list_problems_of_user(session, user_id);
}

ProblemCapabilities problem(const decltype(Context::session)& session,
        decltype(Problem::type) problem_type,
        decltype(Problem::owner_id) problem_owner_id) noexcept {
    bool is_owned = problem_owner_id and is_self(session, *problem_owner_id);
    bool is_public = problem_type == sim::problems::Problem::Type::PUBLIC;
    bool is_contest_only = problem_type == sim::problems::Problem::Type::CONTEST_ONLY;
    bool view = is_public or is_owned or is_admin(session) or
            (is_teacher(session) and is_contest_only);
    return ProblemCapabilities{
            .view = view,
            .view_statement = view,
            .view_public_tags = view,
            .view_hidden_tags =
                    view and (is_admin(session) or is_teacher(session) or is_owned),
            .view_owner = is_admin(session) or is_owned or
                    (is_teacher(session) and (is_public or is_contest_only)),
            .view_simfile = is_admin(session) or is_owned or
                    (is_teacher(session) and (is_public or is_contest_only)),
            .download =
                    is_admin(session) or is_owned or (is_teacher(session) and is_public),
            .reupload = is_admin(session) or is_owned,
            .rejudge_all_submissions = is_admin(session) or is_owned,
            .reset_time_limits = is_admin(session) or is_owned,
            .delete_ = is_admin(session) or is_owned,
            .merge_into_another_problem = is_admin(session) or is_owned,
            .merge_other_problem_into_this_problem = is_admin(session) or is_owned,
    };
}

} // namespace web_server::capabilities

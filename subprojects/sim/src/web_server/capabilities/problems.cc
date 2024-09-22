#include "../web_worker/context.hh"
#include "problems.hh"
#include "utils.hh"

#include <sim/problems/problem.hh>
#include <sim/users/user.hh>

using sim::problems::Problem;
using sim::users::User;
using web_server::web_worker::Context;

namespace web_server::capabilities {

ProblemsCapabilities problems(const decltype(Context::session)& session) noexcept {
    return {
        .web_ui_view = true,
        .add_problem = is_admin(session) || is_teacher(session),
        .add_problem_with_visibility_private = is_admin(session) || is_teacher(session),
        .add_problem_with_visibility_contest_only = is_admin(session) || is_teacher(session),
        .add_problem_with_visibility_public = is_admin(session),
    };
}

ProblemsListCapabilities list_problems(const decltype(Context::session)& session) noexcept {
    return {
        .query_all = true,
        .query_with_visibility_public = true,
        .query_with_visibility_contest_only = is_admin(session) || is_teacher(session) ||
            (session &&
             list_user_problems(session, session->user_id).query_with_visibility_contest_only),
        .query_with_visibility_private = is_admin(session) || is_teacher(session) ||
            (session && list_user_problems(session, session->user_id).query_with_visibility_private
            ),
        .view_all_with_visibility_public = true,
        .view_all_with_visibility_contest_only = is_admin(session) || is_teacher(session),
        .view_all_with_visibility_private = is_admin(session),
    };
}

ProblemsListCapabilities
list_user_problems(const decltype(Context::session)& session, decltype(User::id) user_id) noexcept {
    return {
        .query_all = is_self(session, user_id) || is_admin(session),
        .query_with_visibility_public = is_self(session, user_id) || is_admin(session),
        .query_with_visibility_contest_only = is_self(session, user_id) || is_admin(session),
        .query_with_visibility_private = is_self(session, user_id) || is_admin(session),
        .view_all_with_visibility_public = is_self(session, user_id) || is_admin(session),
        .view_all_with_visibility_contest_only = is_self(session, user_id) || is_admin(session),
        .view_all_with_visibility_private = is_self(session, user_id) || is_admin(session),
    };
}

ProblemCapabilities problem(
    const decltype(Context::session)& session,
    decltype(Problem::visibility) problem_visibility,
    decltype(Problem::owner_id) problem_owner_id
) noexcept {
    bool is_owned = problem_owner_id && is_self(session, *problem_owner_id);
    bool is_public = problem_visibility == sim::problems::Problem::Visibility::PUBLIC;
    bool is_contest_only = problem_visibility == sim::problems::Problem::Visibility::CONTEST_ONLY;
    bool view =
        is_public || is_owned || is_admin(session) || (is_teacher(session) && is_contest_only);
    return {
        .view = view,
        .view_statement = view,
        .view_public_tags = view,
        .view_hidden_tags = view && (is_admin(session) || is_teacher(session) || is_owned),
        .view_solutions = is_admin(session) || is_owned || (is_teacher(session) && is_public),
        .view_simfile = is_admin(session) || is_owned ||
            (is_teacher(session) && (is_public || is_contest_only)),
        .view_owner = is_admin(session) || is_owned ||
            (is_teacher(session) && (is_public || is_contest_only)),
        .view_creation_time = is_admin(session) || is_owned ||
            (is_teacher(session) && (is_public || is_contest_only)),
        .view_update_time = is_admin(session) || is_owned ||
            (is_teacher(session) && (is_public || is_contest_only)),
        .view_final_submission_full_status = view && session,
        .download = is_admin(session) || is_owned || (is_teacher(session) && is_public),
        .create_submission = view && session,
        .edit = is_admin(session) || is_owned,
        .reupload = is_admin(session) || is_owned,
        .rejudge_all_submissions = is_admin(session) || is_owned,
        .reset_time_limits = is_admin(session) || is_owned,
        .delete_ = is_admin(session) || is_owned,
        .merge_into_another_problem = is_admin(session) || is_owned,
        .merge_other_problem_into_this_problem = is_admin(session) || is_owned,
    };
}

} // namespace web_server::capabilities

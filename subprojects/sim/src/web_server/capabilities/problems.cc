#include "../web_worker/context.hh"
#include "problems.hh"
#include "utils.hh"

#include <sim/problems/problem.hh>
#include <sim/users/old_user.hh>

using sim::problems::Problem;
using sim::users::User;
using web_server::web_worker::Context;

namespace web_server::capabilities {

ProblemsCapabilities problems(const decltype(Context::session)& session) noexcept {
    return {
        .web_ui_view = true,
        .add_problem = is_admin(session) or is_teacher(session),
        .add_problem_with_type_private = is_admin(session) or is_teacher(session),
        .add_problem_with_type_contest_only = is_admin(session) or is_teacher(session),
        .add_problem_with_type_public = is_admin(session),
    };
}

ProblemsListCapabilities list_problems(const decltype(Context::session)& session) noexcept {
    return {
        .query_all = true,
        .query_with_type_public = true,
        .query_with_type_contest_only = is_admin(session) or is_teacher(session) or
            (session and list_user_problems(session, session->user_id).query_with_type_contest_only
            ),
        .query_with_type_private = is_admin(session) or is_teacher(session) or
            (session and list_user_problems(session, session->user_id).query_with_type_private),
        .view_all_with_type_public = true,
        .view_all_with_type_contest_only = is_admin(session) or is_teacher(session),
        .view_all_with_type_private = is_admin(session),
        .web_ui_show_owner_column = is_admin(session) or is_teacher(session),
        .web_ui_show_updated_at_column = is_admin(session) or is_teacher(session),
    };
}

ProblemsListCapabilities
list_user_problems(const decltype(Context::session)& session, decltype(User::id) user_id) noexcept {
    return {
        .query_all = is_self(session, user_id) or is_admin(session),
        .query_with_type_public = is_self(session, user_id) or is_admin(session),
        .query_with_type_contest_only = is_self(session, user_id) or is_admin(session),
        .query_with_type_private = is_self(session, user_id) or is_admin(session),
        .view_all_with_type_public = is_self(session, user_id) or is_admin(session),
        .view_all_with_type_contest_only = is_self(session, user_id) or is_admin(session),
        .view_all_with_type_private = is_self(session, user_id) or is_admin(session),
        .web_ui_show_owner_column = is_admin(session) and !is_self(session, user_id),
        .web_ui_show_updated_at_column = is_self(session, user_id) or is_admin(session),
    };
}

ProblemCapabilities problem(
    const decltype(Context::session)& session,
    decltype(Problem::type) problem_type,
    decltype(Problem::owner_id) problem_owner_id
) noexcept {
    bool is_owned = problem_owner_id and is_self(session, *problem_owner_id);
    bool is_public = problem_type == sim::problems::Problem::Type::PUBLIC;
    bool is_contest_only = problem_type == sim::problems::Problem::Type::CONTEST_ONLY;
    bool view =
        is_public or is_owned or is_admin(session) or (is_teacher(session) and is_contest_only);
    return {
        .view = view,
        .view_statement = view,
        .view_public_tags = view,
        .view_hidden_tags = view and (is_admin(session) or is_teacher(session) or is_owned),
        .view_solutions = is_admin(session) or is_owned or (is_teacher(session) and is_public),
        .view_simfile = is_admin(session) or is_owned or
            (is_teacher(session) and (is_public or is_contest_only)),
        .view_owner = is_admin(session) or is_owned or
            (is_teacher(session) and (is_public or is_contest_only)),
        .view_creation_time = is_admin(session) or is_owned or
            (is_teacher(session) and (is_public or is_contest_only)),
        .view_update_time = is_admin(session) or is_owned or
            (is_teacher(session) and (is_public or is_contest_only)),
        .view_final_submission_full_status = view and session,
        .download = is_admin(session) or is_owned or (is_teacher(session) and is_public),
        .create_submission = view and session,
        .edit = is_admin(session) or is_owned,
        .reupload = is_admin(session) or is_owned,
        .rejudge_all_submissions = is_admin(session) or is_owned,
        .reset_time_limits = is_admin(session) or is_owned,
        .delete_ = is_admin(session) or is_owned,
        .merge_into_another_problem = is_admin(session) or is_owned,
        .merge_other_problem_into_this_problem = is_admin(session) or is_owned,
    };
}

} // namespace web_server::capabilities

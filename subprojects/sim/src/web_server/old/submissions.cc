#include "sim.hh"

#include <sim/submissions/old_submission.hh>
#include <sim/users/user.hh>

using sim::contest_users::OldContestUser;
using sim::problems::OldProblem;
using sim::submissions::OldSubmission;
using sim::users::User;

namespace web_server::old {

Sim::SubmissionPermissions Sim::submissions_get_overall_permissions() noexcept {
    using PERM = SubmissionPermissions;

    if (not session.has_value()) {
        return PERM::NONE;
    }

    switch (session->user_type) {
    case User::Type::ADMIN: return PERM::VIEW_ALL;
    case User::Type::TEACHER:
    case User::Type::NORMAL: return PERM::NONE;
    }

    return PERM::NONE; // Shouldn't happen
}

Sim::SubmissionPermissions Sim::submissions_get_permissions(
    decltype(OldSubmission::user_id) submission_user_id,
    OldSubmission::Type stype,
    std::optional<OldContestUser::Mode> cu_mode,
    decltype(OldProblem::owner_id) problem_owner
) noexcept {
    using PERM = SubmissionPermissions;
    using STYPE = OldSubmission::Type;
    using CUM = OldContestUser::Mode;

    PERM overall_perms = submissions_get_overall_permissions();

    if (not session.has_value()) {
        return PERM::NONE;
    }

    static_assert(static_cast<uint>(PERM::NONE) == 0, "Needed below");
    PERM PERM_SUBMISSION_ADMIN = PERM::VIEW | PERM::VIEW_SOURCE | PERM::VIEW_FINAL_REPORT |
        PERM::VIEW_RELATED_JOBS | PERM::REJUDGE |
        (is_one_of(stype, STYPE::NORMAL, STYPE::IGNORED) ? PERM::CHANGE_TYPE | PERM::DELETE
                                                         : PERM::NONE);

    if (session->user_type == User::Type::ADMIN or
        (cu_mode.has_value() and is_one_of(cu_mode.value(), CUM::MODERATOR, CUM::OWNER)))
    {
        return overall_perms | PERM_SUBMISSION_ADMIN;
    }

    // This check has to be done as the last one because it gives the least
    // permissions
    if (session.has_value() and problem_owner and session->user_id == problem_owner.value()) {
        if (stype == STYPE::PROBLEM_SOLUTION) {
            return overall_perms | PERM::VIEW | PERM::VIEW_SOURCE | PERM::VIEW_FINAL_REPORT |
                PERM::VIEW_RELATED_JOBS | PERM::REJUDGE;
        }

        if (submission_user_id and session->user_id == submission_user_id.value()) {
            return overall_perms | PERM_SUBMISSION_ADMIN;
        }

        return overall_perms | PERM::VIEW | PERM::VIEW_SOURCE | PERM::VIEW_FINAL_REPORT |
            PERM::VIEW_RELATED_JOBS | PERM::REJUDGE;
    }

    if (session.has_value() and submission_user_id and
        session->user_id == submission_user_id.value())
    {
        return overall_perms | PERM::VIEW | PERM::VIEW_SOURCE;
    }

    return overall_perms;
}

void Sim::submissions_handle() {
    STACK_UNWINDING_MARK;

    StringView next_arg = url_args.extract_next_arg();
    if (is_digit(next_arg)) { // View submission
        page_template(from_unsafe{concat("Submission ", next_arg)});
        append("view_submission(false, ", next_arg, ", window.location.hash);");

    } else if (next_arg.empty()) { // List submissions
        page_template("Submissions");
        // clang-format off
        append("document.body.appendChild(elem_with_text('h1', 'Submissions'));"
               "$(document).ready(function(){"
                   "var main = $('body');"
                   "var tabs = [];"
                   "if (signed_user_is_admin()) {"
                       "tabs.push('All submissions', function() {"
                           "main.children('.old_tabmenu + div, .loader,.loader-info').remove();"
                           "tab_submissions_lister($('<div>').appendTo(main),'', true);"
                       "});"
                   "}"
                   "if (is_signed_in()) {"
                       "tabs.push('My submissions', function() {"
                           "main.children('.old_tabmenu + div, .loader,.loader-info').remove();"
                           "tab_submissions_lister($('<div>').appendTo(main),"
                               "'/u' + signed_user_id);"
                       "});"
                   "}"
                   "old_tabmenu(function(x) { x.appendTo(main); }, tabs);"
               "});"
        );
        // clang-format on

    } else {
        return error404();
    }
}

} // namespace web_server::old

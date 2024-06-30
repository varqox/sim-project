#include "sim.hh"

#include <sim/contests/old_contest.hh>
#include <sim/jobs/utils.hh>
#include <sim/problems/permissions.hh>

using sim::jobs::OldJob;
using sim::problems::OldProblem;
using sim::submissions::OldSubmission;
using sim::users::User;
using std::optional;
using std::string;

namespace web_server::old {

Sim::JobPermissions Sim::jobs_get_overall_permissions() noexcept {
    STACK_UNWINDING_MARK;
    using PERM = JobPermissions;

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

Sim::JobPermissions Sim::jobs_get_permissions(
    std::optional<StringView> creator_id, OldJob::Type job_type, OldJob::Status job_status
) noexcept {
    STACK_UNWINDING_MARK;
    using PERM = JobPermissions;
    using JT = OldJob::Type;
    using JS = OldJob::Status;

    JobPermissions overall_perms = jobs_get_overall_permissions();

    if (not session.has_value()) {
        return overall_perms;
    }

    static_assert(uint(PERM::NONE) == 0, "Needed below");
    JobPermissions type_perm = [job_type] {
        switch (job_type) {
        case JT::ADD_PROBLEM:
        case JT::REUPLOAD_PROBLEM: return PERM::DOWNLOAD_LOG | PERM::DOWNLOAD_UPLOADED_PACKAGE;

        case JT::CHANGE_PROBLEM_STATEMENT:
            return PERM::DOWNLOAD_LOG | PERM::DOWNLOAD_UPLOADED_STATEMENT;

        case JT::JUDGE_SUBMISSION:
        case JT::REJUDGE_SUBMISSION:
        case JT::EDIT_PROBLEM:
        case JT::DELETE_PROBLEM:
        case JT::MERGE_PROBLEMS:
        case JT::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
        case JT::MERGE_USERS:
        case JT::DELETE_USER:
        case JT::DELETE_CONTEST:
        case JT::DELETE_CONTEST_ROUND:
        case JT::DELETE_CONTEST_PROBLEM:
        case JT::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
        case JT::DELETE_INTERNAL_FILE: return PERM::NONE;
        }

        return PERM::NONE; // Shouldn't happen
    }();

    if (session->user_type == User::Type::ADMIN) {
        switch (job_status) {
        case JS::PENDING:
        case JS::IN_PROGRESS:
            return overall_perms | type_perm | PERM::VIEW | PERM::DOWNLOAD_LOG | PERM::VIEW_ALL |
                PERM::CANCEL;

        case JS::FAILED:
        case JS::CANCELLED:
            return overall_perms | type_perm | PERM::VIEW | PERM::DOWNLOAD_LOG | PERM::VIEW_ALL |
                PERM::RESTART;

        case JS::DONE:
            return overall_perms | type_perm | PERM::VIEW | PERM::DOWNLOAD_LOG | PERM::VIEW_ALL;
        }
    }

    if (creator_id.has_value() and
        session->user_id == str2num<decltype(session->user_id)>(creator_id.value()))
    {
        if (is_one_of(job_status, JS::PENDING, JS::IN_PROGRESS)) {
            return overall_perms | type_perm | PERM::VIEW | PERM::CANCEL;
        }

        return overall_perms | type_perm | PERM::VIEW;
    }

    return overall_perms;
}

Sim::JobPermissions Sim::jobs_granted_permissions_problem(StringView problem_id) {
    STACK_UNWINDING_MARK;
    using PERM = JobPermissions;
    using P_PERMS = sim::problems::Permissions;

    auto problem_perms = sim::problems::get_permissions(
                             mysql,
                             problem_id,
                             (session.has_value() ? optional{session->user_id} : std::nullopt),
                             (session.has_value() ? optional{session->user_type} : std::nullopt)
    )
                             .value_or(P_PERMS::NONE);

    if (uint(problem_perms & P_PERMS::VIEW_RELATED_JOBS)) {
        return PERM::VIEW | PERM::DOWNLOAD_LOG | PERM::DOWNLOAD_UPLOADED_PACKAGE |
            PERM::DOWNLOAD_UPLOADED_STATEMENT;
    }

    return PERM::NONE;
}

Sim::JobPermissions Sim::jobs_granted_permissions_submission(StringView submission_id) {
    STACK_UNWINDING_MARK;
    using PERM = JobPermissions;

    if (not session.has_value()) {
        return PERM::NONE;
    }
    auto old_mysql = old_mysql::ConnectionView{mysql};
    auto stmt = old_mysql.prepare("SELECT s.type, p.owner_id, p.visibility, cu.mode,"
                                  " c.is_public "
                                  "FROM submissions s "
                                  "STRAIGHT_JOIN problems p ON p.id=s.problem_id "
                                  "LEFT JOIN contests c ON c.id=s.contest_id "
                                  "LEFT JOIN contest_users cu ON cu.user_id=?"
                                  " AND cu.contest_id=s.contest_id "
                                  "WHERE s.id=?");
    stmt.bind_and_execute(session->user_id, submission_id);

    EnumVal<OldSubmission::Type> stype{};
    old_mysql::Optional<decltype(OldProblem::owner_id)::value_type> problem_owner_id;
    decltype(OldProblem::visibility) problem_visibility;
    old_mysql::Optional<decltype(sim::contest_users::OldContestUser::mode)> cu_mode;
    old_mysql::Optional<decltype(sim::contests::OldContest::is_public)> is_public;
    stmt.res_bind_all(stype, problem_owner_id, problem_visibility, cu_mode, is_public);
    if (stmt.next()) {
        if (is_public.has_value() and // <-- contest exists
            uint(
                sim::contests::get_permissions(
                    (session.has_value() ? std::optional{session->user_type} : std::nullopt),
                    is_public.value(),
                    cu_mode
                ) &
                sim::contests::Permissions::ADMIN
            ))
        {
            return PERM::VIEW | PERM::DOWNLOAD_LOG;
        }

        // The below check has to be done as the last one because it gives the
        // least permissions
        auto problem_perms = sim::problems::get_permissions(
            (session.has_value() ? optional{session->user_id} : std::nullopt),
            (session.has_value() ? optional{session->user_type} : std::nullopt),
            problem_owner_id,
            problem_visibility
        );
        // Give access to the problem's submissions' jobs to the problem's admin
        if (bool(uint(problem_perms & sim::problems::Permissions::EDIT))) {
            return PERM::VIEW | PERM::DOWNLOAD_LOG;
        }
    }

    return PERM::NONE;
}

void Sim::jobs_handle() {
    STACK_UNWINDING_MARK;

    StringView next_arg = url_args.extract_next_arg();
    if (is_digit(next_arg)) {
        jobs_jid = next_arg;

        page_template(from_unsafe{concat("Job ", jobs_jid)});
        append("view_job(false, ", jobs_jid, ", window.location.hash);");
        return;
    }

    InplaceBuff<32> query_suffix{};
    if (next_arg == "my") {
        query_suffix.append("/u", session->user_id);
    } else if (!next_arg.empty()) {
        return error404();
    }

    /* List jobs */
    page_template("Job queue");

    append("document.body.appendChild(elem_with_text('h1', 'Jobs'));"
           "tab_jobs_lister($('body'));");
}

} // namespace web_server::old

#include "sim.hh"

#include <sim/change_problem_statement_jobs/change_problem_statement_job.hh>
#include <sim/internal_files/internal_file.hh>
#include <sim/jobs/job.hh>
#include <sim/jobs/utils.hh>
#include <sim/sql/sql.hh>
#include <simlib/path.hh>
#include <simlib/time.hh>
#include <simlib/time_format_conversions.hh>
#include <type_traits>

using sim::jobs::OldJob;
using sim::problems::OldProblem;

constexpr bool is_problem_management_job(OldJob::Type x) {
    using JT = OldJob::Type;
    switch (x) {
    case JT::ADD_PROBLEM:
    case JT::REUPLOAD_PROBLEM:
    case JT::EDIT_PROBLEM:
    case JT::DELETE_PROBLEM:
    case JT::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
    case JT::MERGE_PROBLEMS:
    case JT::CHANGE_PROBLEM_STATEMENT: return true;
    case JT::JUDGE_SUBMISSION:
    case JT::REJUDGE_SUBMISSION:
    case JT::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
    case JT::DELETE_USER:
    case JT::DELETE_CONTEST:
    case JT::DELETE_CONTEST_ROUND:
    case JT::DELETE_CONTEST_PROBLEM:
    case JT::DELETE_INTERNAL_FILE:
    case JT::MERGE_USERS: return false;
    }
    return false;
}

constexpr bool is_submission_job(OldJob::Type x) {
    using JT = OldJob::Type;
    switch (x) {
    case JT::JUDGE_SUBMISSION:
    case JT::REJUDGE_SUBMISSION: return true;
    case JT::ADD_PROBLEM:
    case JT::REUPLOAD_PROBLEM:
    case JT::EDIT_PROBLEM:
    case JT::DELETE_PROBLEM:
    case JT::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
    case JT::DELETE_USER:
    case JT::DELETE_CONTEST:
    case JT::DELETE_CONTEST_ROUND:
    case JT::DELETE_CONTEST_PROBLEM:
    case JT::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
    case JT::MERGE_PROBLEMS:
    case JT::DELETE_INTERNAL_FILE:
    case JT::CHANGE_PROBLEM_STATEMENT:
    case JT::MERGE_USERS: return false;
    }
    return false;
}

namespace web_server::old {

static constexpr const char* job_type_str(OldJob::Type type) noexcept {
    using JT = OldJob::Type;

    switch (type) {
    case JT::JUDGE_SUBMISSION: return "Judge submission";
    case JT::REJUDGE_SUBMISSION: return "Rejudge submission";
    case JT::ADD_PROBLEM: return "Add problem";
    case JT::REUPLOAD_PROBLEM: return "Reupload problem";
    case JT::EDIT_PROBLEM: return "Edit problem";
    case JT::DELETE_PROBLEM: return "Delete problem";
    case JT::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
        return "Reselect final submissions in a contest problem";
    case JT::MERGE_USERS: return "Merge users";
    case JT::DELETE_USER: return "Delete user";
    case JT::DELETE_CONTEST: return "Delete contest";
    case JT::DELETE_CONTEST_ROUND: return "Delete contest round";
    case JT::DELETE_CONTEST_PROBLEM: return "Delete contest problem";
    case JT::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
        return "Reset problem time limits using model solution";
    case JT::MERGE_PROBLEMS: return "Merge problems";
    case JT::DELETE_INTERNAL_FILE: return "Delete internal file";
    case JT::CHANGE_PROBLEM_STATEMENT: return "Change problem statement";
    }

    return "Unknown";
}

void Sim::api_jobs() {
    STACK_UNWINDING_MARK;

    if (not session.has_value()) {
        return api_error403();
    }

    using PERM = JobPermissions;

    // We may read data several times (permission checking), so transaction is
    // used to ensure data consistency
    auto transaction = mysql.start_repeatable_read_transaction();

    // Get the overall permissions to the job queue
    jobs_perms = jobs_get_overall_permissions();

    InplaceBuff<512> qfields;
    InplaceBuff<512> qwhere;
    qfields.append("SELECT j.id, j.created_at, j.type, j.status, j.priority, j.aux_id, j.aux_id_2, "
                   "j.creator, u.username");
    qwhere.append(" FROM jobs j LEFT JOIN users u ON creator=u.id WHERE TRUE"
    ); // Needed to easily
       // append constraints

    enum ColumnIdx {
        JID,
        CREATED_AT,
        JTYPE,
        JSTATUS,
        PRIORITY,
        AUX_ID,
        AUX_ID_2,
        CREATOR,
        CREATOR_USERNAME,
        JOB_LOG_VIEW
    };

    auto append_column_names = [&] {
        // clang-format off
        append("[\n{\"columns\":["
                   "\"id\","
                   "\"created_at\","
                   "\"type\","
                   "{\"name\":\"status\",\"fields\":[\"class\",\"text\"]},"
                   "\"priority\","
                   "\"creator_id\","
                   "\"creator_username\","
                   "\"info\","
                   "\"actions\","
                   "{\"name\":\"log\",\"fields\":[\"is_incomplete\",\"text\"]}"
               "]}");
        // clang-format on
    };

    auto set_empty_response = [&] {
        resp.content.clear();
        append_column_names();
        append("\n]");
    };

    bool allow_access = uint(jobs_perms & PERM::VIEW_ALL);
    bool select_specified_job = false;

    PERM granted_perms = PERM::NONE;

    // Process restrictions
    auto rows_limit = API_FIRST_QUERY_ROWS_LIMIT;
    StringView next_arg = url_args.extract_next_arg();
    for (uint mask = 0; !next_arg.empty(); next_arg = url_args.extract_next_arg()) {
        constexpr uint ID_COND = 1;
        constexpr uint AUX_ID_COND = 2;
        constexpr uint USER_ID_COND = 4;

        auto arg = decode_uri(next_arg);
        char cond = arg[0];
        StringView arg_id = StringView(arg).substr(1);

        if (not is_digit(arg_id)) {
            return api_error400();
        }

        // conditional
        if (is_one_of(cond, '<', '>') and ~mask & ID_COND) {
            rows_limit = API_OTHER_QUERY_ROWS_LIMIT;
            qwhere.append(" AND j.id", arg);
            mask |= ID_COND;

        } else if (cond == '=' and ~mask & ID_COND) {
            select_specified_job = true;
            // Get job information to grant permissions
            EnumVal<OldJob::Type> jtype{};
            old_mysql::Optional<decltype(OldJob::aux_id)::value_type> aux_id;
            auto old_mysql = old_mysql::ConnectionView{mysql};
            auto stmt = old_mysql.prepare("SELECT type, aux_id FROM jobs WHERE id=?");
            stmt.bind_and_execute(arg_id);
            stmt.res_bind_all(jtype, aux_id);
            if (not stmt.next()) {
                return api_error404();
            }

            // Grant permissions if possible
            if (is_problem_management_job(jtype) and aux_id) {
                granted_perms |=
                    jobs_granted_permissions_problem(from_unsafe{concat(aux_id.value())});
            } else if (is_submission_job(jtype) and aux_id) {
                granted_perms |=
                    jobs_granted_permissions_submission(from_unsafe{concat(aux_id.value())});
            }
            allow_access |= (granted_perms != PERM::NONE);

            if (not allow_access) {
                // If user cannot view all jobs, allow them to view their jobs
                allow_access = true;
                qwhere.append(" AND creator=", session->user_id);
            }

            qfields.append(", SUBSTR(log, 1, ", sim::jobs::job_log_view_max_size + 1, ')');
            qwhere.append(" AND j.id", arg);
            mask |= ID_COND;

        } else if (cond == 'p' and ~mask & AUX_ID_COND) { // Problem
            // Check permissions - they may be granted
            granted_perms |= jobs_granted_permissions_problem(arg_id);
            allow_access |= (granted_perms != PERM::NONE);

            qwhere.append(
                " AND j.aux_id=",
                arg_id,
                " AND j.type IN(",
                EnumVal(OldJob::Type::ADD_PROBLEM).to_int(),
                ',',
                EnumVal(OldJob::Type::REUPLOAD_PROBLEM).to_int(),
                ',',
                EnumVal(OldJob::Type::DELETE_PROBLEM).to_int(),
                ',',
                EnumVal(OldJob::Type::EDIT_PROBLEM).to_int(),
                ')'
            );

            mask |= AUX_ID_COND;

        } else if (cond == 's' and ~mask & AUX_ID_COND) { // Submission
            // Check permissions - they may be granted
            granted_perms |= jobs_granted_permissions_submission(arg_id);
            allow_access |= (granted_perms != PERM::NONE);

            qwhere.append(
                " AND j.aux_id=",
                arg_id,
                " AND (j.type=",
                EnumVal(OldJob::Type::JUDGE_SUBMISSION).to_int(),
                " OR j.type=",
                EnumVal(OldJob::Type::REJUDGE_SUBMISSION).to_int(),
                ')'
            );

            mask |= AUX_ID_COND;

        } else if (cond == 'u' and ~mask & USER_ID_COND) { // User (creator)
            qwhere.append(" AND creator=", arg_id);
            if (str2num<decltype(session->user_id)>(arg_id) == session->user_id) {
                allow_access = true;
            }

            mask |= USER_ID_COND;

        } else {
            return api_error400();
        }
    }

    if (not allow_access) {
        return set_empty_response();
    }

    // Execute query
    qfields.append(qwhere, " ORDER BY j.id DESC LIMIT ", rows_limit);
    auto old_mysql = old_mysql::ConnectionView{mysql};
    auto res = old_mysql.query(qfields);

    append_column_names();

    while (res.next()) {
        EnumVal<OldJob::Type> job_type{
            WONT_THROW(str2num<OldJob::Type::UnderlyingType>(res[JTYPE]).value())
        };
        EnumVal<OldJob::Status> job_status{
            WONT_THROW(str2num<OldJob::Status::UnderlyingType>(res[JSTATUS]).value())
        };

        // clang-format off
        append(",\n[", res[JID], ","
               "\"", res[CREATED_AT], "\","
               "\"", job_type_str(job_type), "\",");
        // clang-format on

        // Status: (CSS class, text)
        switch (job_status) {
        case OldJob::Status::PENDING: append(R"(["","Pending"],)"); break;
        case OldJob::Status::IN_PROGRESS: append(R"(["yellow","In progress"],)"); break;
        case OldJob::Status::DONE: append(R"(["green","Done"],)"); break;
        case OldJob::Status::FAILED: append(R"(["red","Failed"],)"); break;
        case OldJob::Status::CANCELLED: append(R"(["blue","Cancelled"],)"); break;
        }

        append(res[PRIORITY], ',');

        // Creator
        if (res.is_null(CREATOR)) {
            append("null,");
        } else {
            append(res[CREATOR], ',');
        }

        // Username
        if (res.is_null(CREATOR_USERNAME)) {
            append("null,");
        } else {
            append("\"", res[CREATOR_USERNAME], "\",");
        }

        // Additional info
        InplaceBuff<10> actions;
        append('{');

        switch (job_type) {
        case OldJob::Type::JUDGE_SUBMISSION:
        case OldJob::Type::REJUDGE_SUBMISSION: {
            append("\"submission\":", res[AUX_ID]);
            break;
        }

        case OldJob::Type::ADD_PROBLEM:
        case OldJob::Type::REUPLOAD_PROBLEM: {
            if (not res.is_null(AUX_ID)) {
                append("\"problem\":", res[AUX_ID]);
                actions.append('p'); // View problem
            }

            break;
        }

        case OldJob::Type::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM: {
            append("\"contest problem\":", res[AUX_ID]);
            break;
        }

        case OldJob::Type::DELETE_PROBLEM: {
            append("\"problem\":", res[AUX_ID]);
            break;
        }

        case OldJob::Type::MERGE_USERS: {
            append("\"deleted user\":", res[AUX_ID]);
            append(",\"target user\":", res[AUX_ID_2]);
            break;
        }

        case OldJob::Type::DELETE_USER: {
            append("\"user\":", res[AUX_ID]);
            break;
        }

        case OldJob::Type::DELETE_CONTEST: {
            append("\"contest\":", res[AUX_ID]);
            break;
        }

        case OldJob::Type::DELETE_CONTEST_ROUND: {
            append("\"contest round\":", res[AUX_ID]);
            break;
        }

        case OldJob::Type::DELETE_CONTEST_PROBLEM: {
            append("\"contest problem\":", res[AUX_ID]);
            break;
        }

        case OldJob::Type::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION: {
            append("\"problem\":", res[AUX_ID]);
            break;
        }

        case OldJob::Type::MERGE_PROBLEMS: {
            append("\"deleted problem\":", res[AUX_ID]);
            append(",\"target problem\":", res[AUX_ID_2]);
            break;
        }

        case OldJob::Type::CHANGE_PROBLEM_STATEMENT: {
            append("\"problem\":", res[AUX_ID]);
            break;
        }

        case OldJob::Type::DELETE_INTERNAL_FILE: // Nothing to show
        case OldJob::Type::EDIT_PROBLEM: break;
        }
        append("},");

        // Append what buttons to show
        append('"', actions);

        auto perms =
            granted_perms | jobs_get_permissions(res.to_opt(CREATOR), job_type, job_status);
        if (uint(perms & PERM::VIEW)) {
            append('v');
        }
        if (uint(perms & PERM::DOWNLOAD_LOG)) {
            append('r');
        }
        using JT = OldJob::Type;
        if (uint(perms & PERM::DOWNLOAD_UPLOADED_PACKAGE) and
            is_one_of(job_type, JT::ADD_PROBLEM, JT::REUPLOAD_PROBLEM))
        {
            append('u'); // TODO: ^ that is very nasty
        }
        if (uint(perms & PERM::DOWNLOAD_UPLOADED_STATEMENT) and
            job_type == JT::CHANGE_PROBLEM_STATEMENT)
        {
            append('s'); // TODO: ^ that is very nasty
        }
        if (uint(perms & PERM::CANCEL)) {
            append('C');
        }
        if (uint(perms & PERM::RESTART)) {
            append('R');
        }
        append('\"');

        // Append log view (whether there is more to load, log)
        if (select_specified_job and uint(perms & PERM::DOWNLOAD_LOG)) {
            append(
                ",[",
                res[JOB_LOG_VIEW].size() > sim::jobs::job_log_view_max_size,
                ',',
                json_stringify(res[JOB_LOG_VIEW]),
                ']'
            );
        }

        append(']');
    }

    append("\n]");
}

void Sim::api_job() {
    STACK_UNWINDING_MARK;
    using JT = OldJob::Type;

    if (not session.has_value()) {
        return api_error403();
    }

    jobs_jid = url_args.extract_next_arg();
    if (not is_digit(jobs_jid)) {
        return api_error400();
    }

    old_mysql::Optional<InplaceBuff<32>> jcreator;
    old_mysql::Optional<decltype(OldJob::aux_id)::value_type> aux_id;
    EnumVal<JT> jtype{};
    EnumVal<OldJob::Status> jstatus{};

    auto old_mysql = old_mysql::ConnectionView{mysql};
    auto stmt = old_mysql.prepare("SELECT creator, type, status, aux_id "
                                  "FROM jobs WHERE id=?");
    stmt.bind_and_execute(jobs_jid);
    stmt.res_bind_all(jcreator, jtype, jstatus, aux_id);
    if (not stmt.next()) {
        return api_error404();
    }

    {
        std::optional<StringView> creator;
        if (jcreator.has_value()) {
            creator = jcreator.value();
        }
        jobs_perms = jobs_get_permissions(creator, jtype, jstatus);
    }
    // Grant permissions if possible
    if (is_problem_management_job(jtype) and aux_id) {
        jobs_perms |= jobs_granted_permissions_problem(from_unsafe{concat(aux_id.value())});
    } else if (is_submission_job(jtype) and aux_id) {
        jobs_perms |= jobs_granted_permissions_submission(from_unsafe{concat(aux_id.value())});
    }

    StringView next_arg = url_args.extract_next_arg();
    if (next_arg == "cancel") {
        return api_job_cancel();
    }
    if (next_arg == "restart") {
        return api_job_restart();
    }
    if (next_arg == "log") {
        return api_job_download_log();
    }
    if (next_arg == "uploaded-package") {
        return api_job_download_uploaded_package(
            str2num<decltype(sim::jobs::Job::id)>(jobs_jid).value(), jtype
        );
    }
    if (next_arg == "uploaded-statement") {
        return api_job_download_uploaded_statement(
            str2num<decltype(sim::jobs::Job::id)>(jobs_jid).value(), jtype
        );
    }
    return api_error400();
}

void Sim::api_job_cancel() {
    STACK_UNWINDING_MARK;
    using PERM = JobPermissions;

    if (request.method != http::Request::POST) {
        return api_error400();
    }

    if (uint(~jobs_perms & PERM::CANCEL)) {
        if (uint(jobs_perms & PERM::VIEW)) {
            return api_error400("Job has already been cancelled or done");
        }
        return api_error403();
    }

    // Cancel job
    auto old_mysql = old_mysql::ConnectionView{mysql};
    old_mysql.prepare("UPDATE jobs SET status=? WHERE id=?")
        .bind_and_execute(EnumVal(OldJob::Status::CANCELLED), jobs_jid);
}

void Sim::api_job_restart() {
    STACK_UNWINDING_MARK;
    using PERM = JobPermissions;

    if (request.method != http::Request::POST) {
        return api_error400();
    }

    if (uint(~jobs_perms & PERM::RESTART)) {
        if (uint(jobs_perms & PERM::VIEW)) {
            return api_error400("Job has already been restarted");
        }
        return api_error403();
    }

    sim::jobs::restart_job(mysql, jobs_jid, true);
}

void Sim::api_job_download_log() {
    STACK_UNWINDING_MARK;
    using PERM = JobPermissions;

    if (uint(~jobs_perms & PERM::DOWNLOAD_LOG)) {
        return api_error403();
    }

    // Assumption: permissions are already checked
    resp.headers["Content-type"] = "application/text";
    resp.headers["Content-Disposition"] =
        concat_tostr("attachment; filename=job-", jobs_jid, "-log");

    // Fetch the log
    auto old_mysql = old_mysql::ConnectionView{mysql};
    auto stmt = old_mysql.prepare("SELECT log FROM jobs WHERE id=?");
    stmt.bind_and_execute(jobs_jid);
    stmt.res_bind_all(resp.content);
    throw_assert(stmt.next());
}

void Sim::api_job_download_uploaded_package(uint64_t job_id, OldJob::Type job_type) {
    STACK_UNWINDING_MARK;
    using PERM = JobPermissions;
    using JT = OldJob::Type;

    if (uint(~jobs_perms & PERM::DOWNLOAD_UPLOADED_PACKAGE) or
        not is_one_of(job_type, JT::ADD_PROBLEM, JT::REUPLOAD_PROBLEM))
    {
        return api_error403(); // TODO: ^ that is very nasty
    }

    auto stmt = mysql.execute(
        sim::sql::Select("file_id")
            .from(job_type == JT::ADD_PROBLEM ? "add_problem_jobs" : "reupload_problem_jobs")
            .where("id=?", job_id)
    );
    decltype(sim::internal_files::InternalFile::id) file_id;
    stmt.res_bind(file_id);
    throw_assert(stmt.next());

    resp.headers["Content-Disposition"] = concat_tostr("attachment; filename=", job_id, ".zip");
    resp.content_type = http::Response::FILE;
    resp.content = sim::internal_files::path_of(file_id);
}

void Sim::api_job_download_uploaded_statement(uint64_t job_id, OldJob::Type job_type) {
    STACK_UNWINDING_MARK;
    using PERM = JobPermissions;
    using JT = OldJob::Type;

    if (uint(~jobs_perms & PERM::DOWNLOAD_UPLOADED_STATEMENT) or
        job_type != JT::CHANGE_PROBLEM_STATEMENT)
    {
        return api_error403(); // TODO: ^ that is very nasty
    }

    decltype(sim::change_problem_statement_jobs::ChangeProblemStatementJob::new_statement_file_id
    ) new_statement_file_id;
    decltype(sim::change_problem_statement_jobs::ChangeProblemStatementJob::path_for_new_statement
    ) path_for_new_statement;
    auto stmt = mysql.execute(sim::sql::Select("new_statement_file_id, path_for_new_statement")
                                  .from("change_problem_statement_jobs")
                                  .where("id=?", job_id));
    stmt.res_bind(new_statement_file_id, path_for_new_statement);
    throw_assert(stmt.next());

    resp.headers["Content-Disposition"] = concat_tostr(
        "attachment; filename=",
        encode_uri(from_unsafe{path_filename(from_unsafe{path_for_new_statement})})
    );
    resp.content_type = http::Response::FILE;
    resp.content = sim::internal_files::path_of(new_statement_file_id);
}

} // namespace web_server::old

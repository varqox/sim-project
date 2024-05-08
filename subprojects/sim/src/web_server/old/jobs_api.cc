#include "sim.hh"

#include <sim/jobs/utils.hh>
#include <simlib/path.hh>
#include <simlib/time.hh>
#include <simlib/time_format_conversions.hh>
#include <type_traits>

using sim::jobs::OldJob;
using sim::problems::OldProblem;

namespace web_server::old {

static constexpr const char* job_type_str(OldJob::Type type) noexcept {
    using JT = OldJob::Type;

    switch (type) {
    case JT::JUDGE_SUBMISSION: return "Judge submission";
    case JT::REJUDGE_SUBMISSION: return "Rejudge submission";
    case JT::ADD_PROBLEM: return "Add problem";
    case JT::REUPLOAD_PROBLEM: return "Reupload problem";
    case JT::ADD_PROBLEM__JUDGE_MODEL_SOLUTION: return "Add problem - set time limits";
    case JT::REUPLOAD_PROBLEM__JUDGE_MODEL_SOLUTION: return "Reupload problem - set time limits";
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
    case JT::DELETE_FILE: return "Delete internal file";
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
    qfields.append("SELECT j.id, j.created_at, j.type, j.status, j.priority, j.aux_id,"
                   " j.info, j.creator, u.username");
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
        JINFO,
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

            qfields.append(", SUBSTR(data, 1, ", sim::jobs::job_log_view_max_size + 1, ')');
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
                EnumVal(OldJob::Type::ADD_PROBLEM__JUDGE_MODEL_SOLUTION).to_int(),
                ',',
                EnumVal(OldJob::Type::REUPLOAD_PROBLEM).to_int(),
                ',',
                EnumVal(OldJob::Type::REUPLOAD_PROBLEM__JUDGE_MODEL_SOLUTION).to_int(),
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
        case OldJob::Status::PENDING:
        case OldJob::Status::NOTICED_PENDING: append(R"(["","Pending"],)"); break;
        case OldJob::Status::IN_PROGRESS: append(R"(["yellow","In progress"],)"); break;
        case OldJob::Status::DONE: append(R"(["green","Done"],)"); break;
        case OldJob::Status::FAILED: append(R"(["red","Failed"],)"); break;
        case OldJob::Status::CANCELED: append(R"(["blue","Canceled"],)"); break;
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
            append("\"problem\":", sim::jobs::extract_dumped_string(res[JINFO]));
            append(",\"submission\":", res[AUX_ID]);
            break;
        }

        case OldJob::Type::ADD_PROBLEM:
        case OldJob::Type::REUPLOAD_PROBLEM:
        case OldJob::Type::ADD_PROBLEM__JUDGE_MODEL_SOLUTION:
        case OldJob::Type::REUPLOAD_PROBLEM__JUDGE_MODEL_SOLUTION: {
            auto ptype_to_str = [](OldProblem::Type& ptype) {
                switch (ptype) {
                case OldProblem::Type::PUBLIC: return "public";
                case OldProblem::Type::PRIVATE: return "private";
                case OldProblem::Type::CONTEST_ONLY: return "contest only";
                }
                return "unknown";
            };
            sim::jobs::AddProblemInfo info{res[JINFO]};
            append(R"("problem type":")", ptype_to_str(info.problem_type), '"');

            if (!info.name.empty()) {
                append(",\"name\":", json_stringify(info.name));
            }
            if (!info.label.empty()) {
                append(",\"label\":", json_stringify(info.label));
            }
            if (info.memory_limit.has_value()) {
                append(R"(,"memory limit":")", info.memory_limit.value(), " MiB\"");
            }
            if (info.global_time_limit.has_value()) {
                append(",\"global time limit\":", to_string(info.global_time_limit.value()));
            }

            append(
                ",\"reset time limits\":",
                info.reset_time_limits ? "\"yes\"" : "\"no\"",
                ",\"ignore simfile\":",
                info.ignore_simfile ? "\"yes\"" : "\"no\"",
                ",\"seek for new tests\":",
                info.seek_for_new_tests ? "\"yes\"" : "\"no\"",
                ",\"reset scoring\":",
                info.reset_scoring ? "\"yes\"" : "\"no\""
            );

            if (not res.is_null(AUX_ID)) {
                append(",\"problem\":", res[AUX_ID]);
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
            sim::jobs::MergeUsersInfo info(res[JINFO]);
            append(",\"target user\":", info.target_user_id);
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
            sim::jobs::MergeProblemsInfo info(res[JINFO]);
            append(",\"target problem\":", info.target_problem_id);
            append(
                ",\"rejudge transferred submissions\":",
                info.rejudge_transferred_submissions ? "\"yes\"" : "\"no\""
            );
            break;
        }

        case OldJob::Type::CHANGE_PROBLEM_STATEMENT: {
            append("\"problem\":", res[AUX_ID]);
            sim::jobs::ChangeProblemStatementInfo info(res[JINFO]);
            append(",\"new statement path\":", json_stringify(info.new_statement_path));
            break;
        }

        case OldJob::Type::DELETE_FILE: // Nothing to show
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
            is_one_of(
                job_type,
                JT::ADD_PROBLEM,
                JT::REUPLOAD_PROBLEM,
                JT::ADD_PROBLEM__JUDGE_MODEL_SOLUTION,
                JT::REUPLOAD_PROBLEM__JUDGE_MODEL_SOLUTION
            ))
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

        // Append log view (whether there is more to load, data)
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

    old_mysql::Optional<uint64_t> file_id;
    old_mysql::Optional<InplaceBuff<32>> jcreator;
    old_mysql::Optional<decltype(OldJob::aux_id)::value_type> aux_id;
    InplaceBuff<256> jinfo;
    EnumVal<JT> jtype{};
    EnumVal<OldJob::Status> jstatus{};

    auto old_mysql = old_mysql::ConnectionView{mysql};
    auto stmt = old_mysql.prepare("SELECT file_id, creator, type, status, aux_id, info "
                                  "FROM jobs WHERE id=?");
    stmt.bind_and_execute(jobs_jid);
    stmt.res_bind_all(file_id, jcreator, jtype, jstatus, aux_id, jinfo);
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
        return api_job_restart(jtype, jinfo);
    }
    if (next_arg == "log") {
        return api_job_download_log();
    }
    if (next_arg == "uploaded-package") {
        return api_job_download_uploaded_package(file_id, jtype);
    }
    if (next_arg == "uploaded-statement") {
        return api_job_download_uploaded_statement(file_id, jtype, jinfo);
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
            return api_error400("Job has already been canceled or done");
        }
        return api_error403();
    }

    // Cancel job
    auto old_mysql = old_mysql::ConnectionView{mysql};
    old_mysql.prepare("UPDATE jobs SET status=? WHERE id=?")
        .bind_and_execute(EnumVal(OldJob::Status::CANCELED), jobs_jid);
}

void Sim::api_job_restart(OldJob::Type job_type, StringView job_info) {
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

    sim::jobs::restart_job(mysql, jobs_jid, job_type, job_info, true);
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
    auto stmt = old_mysql.prepare("SELECT data FROM jobs WHERE id=?");
    stmt.bind_and_execute(jobs_jid);
    stmt.res_bind_all(resp.content);
    throw_assert(stmt.next());
}

void Sim::api_job_download_uploaded_package(
    std::optional<uint64_t> file_id, OldJob::Type job_type
) {
    STACK_UNWINDING_MARK;
    using PERM = JobPermissions;
    using JT = OldJob::Type;

    if (uint(~jobs_perms & PERM::DOWNLOAD_UPLOADED_PACKAGE) or
        not is_one_of(
            job_type,
            JT::ADD_PROBLEM,
            JT::REUPLOAD_PROBLEM,
            JT::ADD_PROBLEM__JUDGE_MODEL_SOLUTION,
            JT::REUPLOAD_PROBLEM__JUDGE_MODEL_SOLUTION
        ))
    {
        return api_error403(); // TODO: ^ that is very nasty
    }

    resp.headers["Content-Disposition"] = concat_tostr("attachment; filename=", jobs_jid, ".zip");
    resp.content_type = http::Response::FILE;
    resp.content = sim::internal_files::path_of(file_id.value());
}

void Sim::api_job_download_uploaded_statement(
    std::optional<uint64_t> file_id, OldJob::Type job_type, StringView info
) {
    STACK_UNWINDING_MARK;
    using PERM = JobPermissions;
    using JT = OldJob::Type;

    if (uint(~jobs_perms & PERM::DOWNLOAD_UPLOADED_STATEMENT) or
        job_type != JT::CHANGE_PROBLEM_STATEMENT)
    {
        return api_error403(); // TODO: ^ that is very nasty
    }

    resp.headers["Content-Disposition"] = concat_tostr(
        "attachment; filename=",
        encode_uri(from_unsafe{path_filename(
            from_unsafe{sim::jobs::ChangeProblemStatementInfo(info).new_statement_path}
        )})
    );
    resp.content_type = http::Response::FILE;
    resp.content = sim::internal_files::path_of(file_id.value());
}

} // namespace web_server::old

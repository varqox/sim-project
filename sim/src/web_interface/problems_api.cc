#include "sim/constants.hh"
#include "sim/jobs.hh"
#include "sim/problem.hh"
#include "sim/problem_permissions.hh"
#include "simlib/config_file.hh"
#include "simlib/file_info.hh"
#include "simlib/file_manip.hh"
#include "simlib/humanize.hh"
#include "simlib/libzip.hh"
#include "simlib/sim/problem_package.hh"
#include "src/web_interface/sim.hh"

#include <cstdint>
#include <type_traits>

using sim::Problem;
using sim::User;

static constexpr const char* proiblem_type_str(Problem::Type type) noexcept {
    using PT = Problem::Type;

    switch (type) {
    case PT::PUBLIC: return "Public";
    case PT::PRIVATE: return "Private";
    case PT::CONTEST_ONLY: return "Contest only";
    }
    return "Unknown";
}

void Sim::api_problems() {
    STACK_UNWINDING_MARK;

    using OPERMS = sim::problem::OverallPermissions;

    // We may read data several times (permission checking), so transaction is
    // used to ensure data consistency
    auto transaction = mysql.start_transaction();

    InplaceBuff<512> qfields;
    InplaceBuff<512> qwhere;
    qfields.append("SELECT p.id, p.added, p.type, p.name, p.label, p.owner, "
                   "u.username, s.full_status");
    qwhere.append(
        " FROM problems p LEFT JOIN users u ON p.owner=u.id "
        "LEFT JOIN submissions s ON s.owner=",
        (session_is_open ? StringView(session_user_id) : "''"),
        " AND s.problem_id=p.id AND s.problem_final=1 "
        "WHERE TRUE"); // Needed to easily append constraints

    enum ColumnIdx {
        PID,
        ADDED,
        PTYPE,
        NAME,
        LABEL,
        OWNER,
        OWN_USERNAME,
        SFULL_STATUS,
        SIMFILE
    };

    auto overall_perms = sim::problem::get_overall_permissions(
        session_is_open ? std::optional{session_user_type} : std::nullopt);

    // Choose problems to select
    if (not uint(overall_perms & OPERMS::VIEW_ALL)) {
        if (not session_is_open) {
            throw_assert(uint(overall_perms & OPERMS::VIEW_WITH_TYPE_PUBLIC));
            qwhere.append(" AND p.type=", EnumVal(Problem::Type::PUBLIC).int_val());

        } else if (session_user_type == User::Type::TEACHER) {
            throw_assert(
                uint(overall_perms & OPERMS::VIEW_WITH_TYPE_PUBLIC) and
                uint(overall_perms & OPERMS::VIEW_WITH_TYPE_CONTEST_ONLY));
            qwhere.append(
                " AND (p.type=", EnumVal(Problem::Type::PUBLIC).int_val(),
                " OR p.type=", EnumVal(Problem::Type::CONTEST_ONLY).int_val(),
                " OR p.owner=", session_user_id, ')');

        } else {
            throw_assert(uint(overall_perms & OPERMS::VIEW_WITH_TYPE_PUBLIC));
            qwhere.append(
                " AND (p.type=", EnumVal(Problem::Type::PUBLIC).int_val(),
                " OR p.owner=", session_user_id, ')');
        }
    }

    // Process restrictions
    auto rows_limit = API_FIRST_QUERY_ROWS_LIMIT;
    bool select_specified_problem = false;
    StringView next_arg = url_args.extract_next_arg();
    for (uint mask = 0; !next_arg.empty(); next_arg = url_args.extract_next_arg()) {
        constexpr uint ID_COND = 1;
        constexpr uint PTYPE_COND = 2;
        constexpr uint USER_ID_COND = 4;

        auto arg = decode_uri(next_arg);
        char cond = arg[0];
        StringView arg_id = StringView(arg).substr(1);

        // problem type
        if (cond == 't' and ~mask & PTYPE_COND) {
            if (arg_id == "PUB") {
                qwhere.append(" AND p.type=", EnumVal(Problem::Type::PUBLIC).int_val());
            } else if (arg_id == "PRI") {
                qwhere.append(" AND p.type=", EnumVal(Problem::Type::PRIVATE).int_val());
            } else if (arg_id == "CON") {
                qwhere.append(" AND p.type=", EnumVal(Problem::Type::CONTEST_ONLY).int_val());
            } else {
                return api_error400(
                    intentional_unsafe_string_view(concat("Invalid problem type: ", arg_id)));
            }

            mask |= PTYPE_COND;

            // NOLINTNEXTLINE(bugprone-branch-clone)
        } else if (not is_digit(arg_id)) {
            return api_error400();

        } else if (is_one_of(cond, '<', '>') and ~mask & ID_COND) { // Conditional
            rows_limit = API_OTHER_QUERY_ROWS_LIMIT;
            qwhere.append(" AND p.id", arg);
            mask |= ID_COND;

        } else if (cond == '=' and ~mask & ID_COND) {
            qfields.append(", p.simfile");
            select_specified_problem = true;
            qwhere.append(" AND p.id", arg);
            mask |= ID_COND;

        } else if (cond == 'u' and ~mask & USER_ID_COND) { // User (owner)
            // Prevent bypassing unset VIEW_OWNER permission
            if (not session_is_open or
                (arg_id != session_user_id and
                 not uint(overall_perms & OPERMS::SELECT_BY_OWNER)))
            {
                return api_error403("You have no permissions to select others' "
                                    "problems by their user id");
            }

            qwhere.append(" AND p.owner=", arg_id);
            mask |= USER_ID_COND;

        } else {
            return api_error400();
        }
    }

    // Execute query
    qfields.append(qwhere, " ORDER BY p.id DESC LIMIT ", rows_limit);
    auto res = mysql.query(qfields);

    // Column names
    // clang-format off
	append("[\n{\"columns\":["
	           "\"id\","
	           "\"added\","
	           "\"type\","
	           "\"name\","
	           "\"label\","
	           "\"owner_id\","
	           "\"owner_username\","
	           "\"actions\","
	           "{\"name\":\"tags\",\"fields\":["
	               "\"public\","
	               "\"hidden\""
	           "]},"
	           "\"color_class\","
	           "\"simfile\","
	           "\"memory_limit\""
	       "]}");
    // clang-format on

    // Tags selector
    uint64_t pid = 0;
    uint8_t hidden = false;
    InplaceBuff<PROBLEM_TAG_MAX_LEN> tag;
    auto stmt = mysql.prepare("SELECT tag FROM problem_tags "
                              "WHERE problem_id=? AND hidden=? ORDER BY tag");
    stmt.bind_all(pid, hidden);
    stmt.res_bind_all(tag);

    while (res.next()) {
        EnumVal<Problem::Type> problem_type{
            WONT_THROW(str2num<std::underlying_type_t<Problem::Type>>(res[PTYPE]).value())};
        auto problem_perms = sim::problem::get_permissions(
            (session_is_open
                 ? std::optional{WONT_THROW(str2num<uintmax_t>(session_user_id).value())}
                 : std::nullopt),
            (session_is_open ? std::optional{session_user_type} : std::nullopt),
            (res.is_null(OWNER)
                 ? std::nullopt
                 : decltype(Problem::owner)(WONT_THROW(
                       str2num<decltype(Problem::owner)::value_type>(res[OWNER]).value()))),
            problem_type);
        using PERMS = sim::problem::Permissions;

        // Id
        append(",\n[", res[PID], ",");

        // Added
        if (uint(problem_perms & PERMS::VIEW_ADD_TIME)) {
            append("\"", res[ADDED], "\",");
        } else {
            append("null,");
        }

        // Type
        append("\"", proiblem_type_str(problem_type), "\",");
        // Name
        append(json_stringify(res[NAME]), ',');
        // Label
        append(json_stringify(res[LABEL]), ',');

        // Owner
        if (uint(problem_perms & PERMS::VIEW_OWNER) and not res.is_null(OWNER)) {
            append(res[OWNER], ",\"", res[OWN_USERNAME], "\",");
        } else {
            append("null,null,");
        }

        // Append what buttons to show
        append('"');
        if (uint(problem_perms & PERMS::VIEW)) {
            append('v');
        }
        if (uint(problem_perms & PERMS::VIEW_STATEMENT)) {
            append('V');
        }
        if (uint(problem_perms & PERMS::VIEW_TAGS)) {
            append('t');
        }
        if (uint(problem_perms & PERMS::VIEW_HIDDEN_TAGS)) {
            append('h');
        }
        if (uint(problem_perms & PERMS::VIEW_SOLUTIONS_AND_SUBMISSIONS)) {
            append('s');
        }
        if (uint(problem_perms & PERMS::VIEW_SIMFILE)) {
            append('f');
        }
        if (uint(problem_perms & PERMS::VIEW_OWNER)) {
            append('o');
        }
        if (uint(problem_perms & PERMS::VIEW_ADD_TIME)) {
            append('a');
        }
        if (uint(problem_perms & PERMS::VIEW_RELATED_JOBS)) {
            append('j');
        }
        if (uint(problem_perms & PERMS::DOWNLOAD)) {
            append('d');
        }
        if (uint(problem_perms & PERMS::SUBMIT)) {
            append('S');
        }
        if (uint(problem_perms & PERMS::EDIT)) {
            append('E');
        }
        if (uint(problem_perms & PERMS::SUBMIT_IGNORED)) {
            append('i');
        }
        if (uint(problem_perms & PERMS::REUPLOAD)) {
            append('R');
        }
        if (uint(problem_perms & PERMS::REJUDGE_ALL)) {
            append('L');
        }
        if (uint(problem_perms & PERMS::RESET_TIME_LIMITS)) {
            append('J');
        }
        if (uint(problem_perms & PERMS::EDIT_TAGS)) {
            append('T');
        }
        if (uint(problem_perms & PERMS::EDIT_HIDDEN_TAGS)) {
            append('H');
        }
        if (uint(problem_perms & PERMS::CHANGE_STATEMENT)) {
            append('C');
        }
        if (uint(problem_perms & PERMS::DELETE)) {
            append('D');
        }
        if (uint(problem_perms & PERMS::MERGE)) {
            append('M');
        }
        if (uint(problem_perms & PERMS::VIEW_ATTACHING_CONTEST_PROBLEMS)) {
            append('c');
        }
        append("\",");

        auto append_tags = [&](bool h) {
            pid = WONT_THROW(str2num<decltype(pid)>(res[PID]).value());
            hidden = h;
            stmt.execute();
            if (stmt.next()) {
                for (;;) {
                    append(json_stringify(tag));
                    // Select next or break
                    if (stmt.next()) {
                        append(',');
                    } else {
                        break;
                    }
                }
            }
        };

        // Tags
        append("[[");
        // Normal
        if (uint(problem_perms & PERMS::VIEW_TAGS)) {
            append_tags(false);
        }
        append("],[");
        // Hidden
        if (uint(problem_perms & PERMS::VIEW_HIDDEN_TAGS)) {
            append_tags(true);
        }
        append("]]");

        // Append color class
        if (res.is_null(SFULL_STATUS)) {
            append(",null");
        } else {
            append(
                ",\"",
                css_color_class(EnumVal<SubmissionStatus>(WONT_THROW(
                    str2num<std::underlying_type_t<SubmissionStatus>>(res[SFULL_STATUS])
                        .value()))),
                "\"");
        }

        // Append simfile and memory limit
        if (select_specified_problem and uint(problem_perms & PERMS::VIEW_SIMFILE)) {
            ConfigFile cf;
            cf.add_vars("memory_limit");
            cf.load_config_from_string(res[SIMFILE].to_string());
            append(
                ',', json_stringify(res[SIMFILE]), // simfile
                ',', json_stringify(cf.get_var("memory_limit").as_string()));
        }

        append(']');
    }

    append("\n]");
}

void Sim::api_problem() {
    STACK_UNWINDING_MARK;

    auto overall_perms = sim::problem::get_overall_permissions(
        session_is_open ? std::optional{session_user_type} : std::nullopt);

    StringView next_arg = url_args.extract_next_arg();
    if (next_arg == "add") {
        return api_problem_add(overall_perms);
    }
    if (not is_digit(next_arg)) {
        return api_error400();
    }

    problems_pid = next_arg;

    MySQL::Optional<decltype(Problem::owner)::value_type> problem_owner;
    decltype(Problem::label) problem_label;
    decltype(Problem::simfile) problem_simfile;
    decltype(Problem::type) problem_type;

    auto stmt = mysql.prepare("SELECT file_id, owner, type, label, simfile "
                              "FROM problems WHERE id=?");
    stmt.bind_and_execute(problems_pid);
    stmt.res_bind_all(
        problems_file_id, problem_owner, problem_type, problem_label, problem_simfile);
    if (not stmt.next()) {
        return api_error404();
    }

    auto problem_perms = sim::problem::get_permissions(
        (session_is_open
             ? std::optional{WONT_THROW(str2num<uintmax_t>(session_user_id).value())}
             : std::nullopt),
        (session_is_open ? std::optional{session_user_type} : std::nullopt), problem_owner,
        problem_type);

    next_arg = url_args.extract_next_arg();
    if (next_arg == "statement") {
        return api_problem_statement(problem_label, problem_simfile, problem_perms);
    }
    if (next_arg == "download") {
        return api_problem_download(problem_label, problem_perms);
    }
    if (next_arg == "rejudge_all_submissions") {
        return api_problem_rejudge_all_submissions(problem_perms);
    }
    if (next_arg == "reset_time_limits") {
        return api_problem_reset_time_limits(problem_perms);
    }
    if (next_arg == "reupload") {
        return api_problem_reupload(problem_perms);
    }
    if (next_arg == "edit") {
        return api_problem_edit(problem_perms);
    }
    if (next_arg == "change_statement") {
        return api_problem_change_statement(problem_perms);
    }
    if (next_arg == "delete") {
        return api_problem_delete(problem_perms);
    }
    if (next_arg == "merge_into_another") {
        return api_problem_merge_into_another(problem_perms);
    }
    if (next_arg == "attaching_contest_problems") {
        return api_problem_attaching_contest_problems(problem_perms);
    }
    return api_error400();
}

void Sim::api_problem_add_or_reupload_impl(bool reuploading) {
    STACK_UNWINDING_MARK;

    // Validate fields
    CStringView name;
    CStringView label;
    CStringView memory_limit;
    CStringView global_time_limit;
    CStringView package_file;
    bool reset_time_limits = request.form_data.exist("reset_time_limits");
    bool ignore_simfile = request.form_data.exist("ignore_simfile");
    bool seek_for_new_tests = request.form_data.exist("seek_for_new_tests");
    bool reset_scoring = request.form_data.exist("reset_scoring");

    form_validate(name, "name", "Problem's name", decltype(Problem::name)::max_len);
    form_validate(label, "label", "Problem's label", decltype(Problem::label)::max_len);
    form_validate(
        memory_limit, "mem_limit", "Memory limit",
        is_digit_not_greater_than<(std::numeric_limits<uint64_t>::max() >> 20)>);
    form_validate_file_path_not_blank(package_file, "package", "Zipped package");
    form_validate(
        global_time_limit, "global_time_limit", "Global time limit",
        is_real); // TODO: add length limit

    using std::chrono::duration;
    using std::chrono::duration_cast;
    using std::chrono::nanoseconds;

    // Convert global_time_limit
    decltype(jobs::AddProblemInfo::global_time_limit) gtl;
    if (not global_time_limit.empty()) {
        gtl = duration_cast<nanoseconds>(
            duration<double>(strtod(global_time_limit.c_str(), nullptr)));
        if (gtl.value() < MIN_TIME_LIMIT) {
            add_notification(
                "error", "Global time limit cannot be lower than ", to_string(MIN_TIME_LIMIT),
                " s");
        }
    }

    // Memory limit
    decltype(jobs::AddProblemInfo::memory_limit) mem_limit;
    if (not memory_limit.empty()) {
        mem_limit = WONT_THROW(str2num<decltype(mem_limit)::value_type>(memory_limit).value());
    }

    // Validate problem type
    StringView ptype_str = request.form_data.get("type");
    Problem::Type ptype = Problem::Type::PRIVATE; // Silence GCC warning
    if (ptype_str == "PRI") {
        ptype = Problem::Type::PRIVATE;
    } else if (ptype_str == "PUB") {
        ptype = Problem::Type::PUBLIC;
    } else if (ptype_str == "CON") {
        ptype = Problem::Type::CONTEST_ONLY;
    } else {
        add_notification("error", "Invalid problem's type");
    }

    if (notifications.size) {
        return api_error400(notifications);
    }

    jobs::AddProblemInfo ap_info{
        name.to_string(), label.to_string(),  mem_limit,     gtl,  reset_time_limits,
        ignore_simfile,   seek_for_new_tests, reset_scoring, ptype};

    auto transaction = mysql.start_transaction();
    mysql.update("INSERT INTO internal_files VALUES()");
    auto job_file_id = mysql.insert_id();
    FileRemover job_file_remover(internal_file_path(job_file_id));

    // Make the uploaded package file the job's file
    if (move(package_file, internal_file_path(job_file_id))) {
        THROW("move()", errmsg());
    }

    EnumVal jtype = (reuploading ? JobType::REUPLOAD_PROBLEM : JobType::ADD_PROBLEM);
    mysql
        .prepare("INSERT jobs(file_id, creator, priority, type, status, added,"
                 " aux_id, info, data) "
                 "VALUES(?, ?, ?, ?, ?, ?, ?, ?, '')")
        .bind_and_execute(
            job_file_id, session_user_id, priority(jtype), jtype, EnumVal(JobStatus::PENDING),
            mysql_date(), (reuploading ? std::optional(problems_pid) : std::nullopt),
            ap_info.dump());

    auto job_id = mysql.insert_id(); // Has to be retrieved before commit

    transaction.commit();
    job_file_remover.cancel();
    jobs::notify_job_server();

    append(job_id);
}

void Sim::api_problem_add(sim::problem::OverallPermissions overall_perms) {
    STACK_UNWINDING_MARK;

    if (uint(~overall_perms & sim::problem::OverallPermissions::ADD)) {
        return api_error403();
    }

    api_problem_add_or_reupload_impl(false);
}

void Sim::api_statement_impl(
    uint64_t problem_file_id, StringView problem_label, StringView simfile) {
    STACK_UNWINDING_MARK;

    ConfigFile cf;
    cf.add_vars("statement");
    cf.load_config_from_string(simfile.to_string());

    auto& statement = cf.get_var("statement").as_string();
    StringView ext;
    if (has_suffix(statement, ".pdf")) {
        ext = ".pdf";
        resp.headers["Content-type"] = "application/pdf";
    } else if (has_one_of_suffixes(statement, ".txt", ".md")) {
        ext = ".md";
        resp.headers["Content-type"] = "text/markdown; charset=utf-8";
    }

    resp.headers["Content-Disposition"] = concat_tostr(
        "inline; filename=",
        http::quote(intentional_unsafe_string_view(concat(problem_label, ext))));

    // TODO: maybe add some cache system for the statements?
    ZipFile zip(internal_file_path(problem_file_id), ZIP_RDONLY);
    resp.content =
        zip.extract_to_str(zip.get_index(concat(sim::zip_package_main_dir(zip), statement)));
}

void Sim::api_problem_statement(
    StringView problem_label, StringView simfile, sim::problem::Permissions perms) {
    STACK_UNWINDING_MARK;

    if (uint(~perms & sim::problem::Permissions::VIEW_STATEMENT)) {
        return api_error403();
    }

    return api_statement_impl(problems_file_id, problem_label, simfile);
}

void Sim::api_problem_download(StringView problem_label, sim::problem::Permissions perms) {
    STACK_UNWINDING_MARK;

    if (uint(~perms & sim::problem::Permissions::DOWNLOAD)) {
        return api_error403();
    }

    resp.headers["Content-Disposition"] =
        concat_tostr("attachment; filename=", problem_label, ".zip");
    resp.content_type = server::HttpResponse::FILE;
    resp.content = internal_file_path(problems_file_id);
}

void Sim::api_problem_rejudge_all_submissions(sim::problem::Permissions perms) {
    STACK_UNWINDING_MARK;

    if (uint(~perms & sim::problem::Permissions::REJUDGE_ALL)) {
        return api_error403();
    }

    mysql
        .prepare("INSERT jobs (creator, status, priority, type, added, aux_id,"
                 " info, data) "
                 "SELECT ?, ?, ?, ?, ?, id, ?, '' "
                 "FROM submissions WHERE problem_id=? ORDER BY id")
        .bind_and_execute(
            session_user_id, EnumVal(JobStatus::PENDING),
            priority(JobType::REJUDGE_SUBMISSION), EnumVal(JobType::REJUDGE_SUBMISSION),
            mysql_date(), jobs::dump_string(problems_pid), problems_pid);

    jobs::notify_job_server();
}

void Sim::api_problem_reset_time_limits(sim::problem::Permissions perms) {
    STACK_UNWINDING_MARK;

    if (uint(~perms & sim::problem::Permissions::RESET_TIME_LIMITS)) {
        return api_error403();
    }

    mysql
        .prepare("INSERT jobs (creator, status, priority, type, added, aux_id,"
                 " info, data) "
                 "VALUES(?, ?, ?, ?, ?, ?, '', '')")
        .bind_and_execute(
            session_user_id, EnumVal(JobStatus::PENDING),
            priority(JobType::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION),
            EnumVal(JobType::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION), mysql_date(),
            problems_pid);

    jobs::notify_job_server();
    append(mysql.insert_id());
}

void Sim::api_problem_delete(sim::problem::Permissions perms) {
    STACK_UNWINDING_MARK;

    if (uint(~perms & sim::problem::Permissions::DELETE)) {
        return api_error403();
    }

    if (not check_submitted_password()) {
        return api_error403("Invalid password");
    }

    // Queue deleting job
    mysql
        .prepare("INSERT jobs (creator, status, priority, type, added, aux_id,"
                 " info, data) "
                 "VALUES(?, ?, ?, ?, ?, ?, '', '')")
        .bind_and_execute(
            session_user_id, EnumVal(JobStatus::PENDING), priority(JobType::DELETE_PROBLEM),
            EnumVal(JobType::DELETE_PROBLEM), mysql_date(), problems_pid);

    jobs::notify_job_server();
    append(mysql.insert_id());
}

void Sim::api_problem_merge_into_another(sim::problem::Permissions perms) {
    STACK_UNWINDING_MARK;

    if (uint(~perms & sim::problem::Permissions::MERGE)) {
        return api_error403();
    }

    InplaceBuff<32> target_problem_id;
    bool rejudge_transferred_submissions =
        request.form_data.exist("rejudge_transferred_submissions");
    form_validate_not_blank(
        target_problem_id, "target_problem", "Target problem ID",
        is_digit_not_greater_than<
            std::numeric_limits<decltype(jobs::MergeProblemsInfo::target_problem_id)>::max()>);

    if (notifications.size) {
        return api_error400(notifications);
    }

    if (problems_pid == target_problem_id) {
        return api_error400("You cannot merge problem with itself");
    }

    auto tp_perms_opt = sim::problem::get_permissions(
        mysql, target_problem_id,
        (session_is_open
             ? std::optional{WONT_THROW(str2num<uintmax_t>(session_user_id).value())}
             : std::nullopt),
        (session_is_open ? std::optional{session_user_type} : std::nullopt));
    if (not tp_perms_opt) {
        return api_error400("Target problem does not exist");
    }

    auto tp_perms = tp_perms_opt.value();
    if (uint(~tp_perms & sim::problem::Permissions::MERGE)) {
        return api_error403("You do not have permission to merge to the target problem");
    }

    if (not check_submitted_password()) {
        return api_error403("Invalid password");
    }

    // Queue merging job
    mysql
        .prepare("INSERT jobs (creator, status, priority, type, added, aux_id,"
                 " info, data) "
                 "VALUES(?, ?, ?, ?, ?, ?, ?, '')")
        .bind_and_execute(
            session_user_id, EnumVal(JobStatus::PENDING), priority(JobType::MERGE_PROBLEMS),
            EnumVal(JobType::MERGE_PROBLEMS), mysql_date(), problems_pid,
            jobs::MergeProblemsInfo(
                WONT_THROW(str2num<uintmax_t>(target_problem_id).value()),
                rejudge_transferred_submissions)
                .dump());

    jobs::notify_job_server();
    append(mysql.insert_id());
}

void Sim::api_problem_reupload(sim::problem::Permissions perms) {
    STACK_UNWINDING_MARK;

    if (uint(~perms & sim::problem::Permissions::REUPLOAD)) {
        return api_error403();
    }

    api_problem_add_or_reupload_impl(true);
}

void Sim::api_problem_edit(sim::problem::Permissions perms) {
    STACK_UNWINDING_MARK;

    StringView next_arg = url_args.extract_next_arg();
    if (next_arg == "tags") {
        return api_problem_edit_tags(perms);
    }

    return api_error400();
}

void Sim::api_problem_edit_tags(sim::problem::Permissions perms) {
    STACK_UNWINDING_MARK;

    auto add_tag = [&] {
        bool hidden = (request.form_data.get("hidden") == "true");
        StringView name;
        form_validate_not_blank(name, "name", "Tag name", PROBLEM_TAG_MAX_LEN);
        if (notifications.size) {
            return api_error400(notifications);
        }

        if (uint(
                ~perms &
                (hidden ? sim::problem::Permissions::EDIT_HIDDEN_TAGS
                        : sim::problem::Permissions::EDIT_TAGS)))
        {
            return api_error403();
        }

        auto stmt = mysql.prepare("INSERT IGNORE "
                                  "INTO problem_tags(problem_id, tag, hidden) "
                                  "VALUES(?, ?, ?)");
        stmt.bind_and_execute(problems_pid, name, hidden);

        if (stmt.affected_rows() == 0) {
            return api_error400("Tag already exist");
        }
    };

    auto edit_tag = [&] {
        bool hidden = (request.form_data.get("hidden") == "true");
        StringView name;
        StringView old_name;
        form_validate_not_blank(name, "name", "Tag name", PROBLEM_TAG_MAX_LEN);
        form_validate_not_blank(old_name, "old_name", "Old tag name", PROBLEM_TAG_MAX_LEN);
        if (notifications.size) {
            return api_error400(notifications);
        }

        if (uint(
                ~perms &
                (hidden ? sim::problem::Permissions::EDIT_HIDDEN_TAGS
                        : sim::problem::Permissions::EDIT_TAGS)))
        {
            return api_error403();
        }

        if (name == old_name) {
            return;
        }

        auto transaction = mysql.start_transaction();
        auto stmt = mysql.prepare("SELECT 1 FROM problem_tags "
                                  "WHERE tag=? AND problem_id=? AND hidden=?");
        stmt.bind_and_execute(name, problems_pid, hidden);
        if (stmt.next()) {
            return api_error400("Tag already exist");
        }

        stmt = mysql.prepare("UPDATE problem_tags SET tag=? "
                             "WHERE problem_id=? AND tag=? AND hidden=?");
        stmt.bind_and_execute(name, problems_pid, old_name, hidden);
        if (stmt.affected_rows() == 0) {
            return api_error400("Tag does not exist");
        }

        transaction.commit();
    };

    auto delete_tag = [&] {
        StringView name;
        bool hidden = (request.form_data.get("hidden") == "true");
        form_validate_not_blank(name, "name", "Tag name", PROBLEM_TAG_MAX_LEN);
        if (notifications.size) {
            return api_error400(notifications);
        }

        if (uint(
                ~perms &
                (hidden ? sim::problem::Permissions::EDIT_HIDDEN_TAGS
                        : sim::problem::Permissions::EDIT_TAGS)))
        {
            return api_error403();
        }

        mysql
            .prepare("DELETE FROM problem_tags "
                     "WHERE problem_id=? AND tag=? AND hidden=?")
            .bind_and_execute(problems_pid, name, hidden);
    };

    StringView next_arg = url_args.extract_next_arg();
    if (next_arg == "add_tag") {
        return add_tag();
    }
    if (next_arg == "edit_tag") {
        return edit_tag();
    }
    if (next_arg == "delete_tag") {
        return delete_tag();
    }
    return api_error400();
}

void Sim::api_problem_change_statement(sim::problem::Permissions perms) {
    STACK_UNWINDING_MARK;

    if (uint(~perms & sim::problem::Permissions::CHANGE_STATEMENT)) {
        return api_error403();
    }

    CStringView statement_path;
    CStringView statement_file;
    form_validate(statement_path, "path", "New statement path");
    form_validate_file_path_not_blank(statement_file, "statement", "New statement");

    if (get_file_size(statement_file) > NEW_STATEMENT_MAX_SIZE) {
        add_notification(
            "error",
            "New statement file is too big (maximum allowed size: ", NEW_STATEMENT_MAX_SIZE,
            " bytes = ", humanize_file_size(NEW_STATEMENT_MAX_SIZE), ')');
    }

    if (notifications.size) {
        return api_error400(notifications);
    }

    jobs::ChangeProblemStatementInfo cps_info(statement_path);

    auto transaction = mysql.start_transaction();
    mysql.update("INSERT INTO internal_files VALUES()");
    auto job_file_id = mysql.insert_id();
    FileRemover job_file_remover(internal_file_path(job_file_id));

    // Make uploaded statement file the job's file
    if (move(statement_file, internal_file_path(job_file_id))) {
        THROW("move()", errmsg());
    }

    mysql
        .prepare("INSERT jobs (file_id, creator, status, priority, type,"
                 " added, aux_id, info, data) "
                 "VALUES(?, ?, ?, ?, ?, ?, ?, ?, '')")
        .bind_and_execute(
            job_file_id, session_user_id, EnumVal(JobStatus::PENDING),
            priority(JobType::CHANGE_PROBLEM_STATEMENT),
            EnumVal(JobType::CHANGE_PROBLEM_STATEMENT), mysql_date(), problems_pid,
            cps_info.dump());

    auto job_id = mysql.insert_id(); // Has to be retrieved before commit

    transaction.commit();
    job_file_remover.cancel();
    jobs::notify_job_server();

    append(job_id);
}

void Sim::api_problem_attaching_contest_problems(sim::problem::Permissions perms) {
    STACK_UNWINDING_MARK;

    if (uint(~perms & sim::problem::Permissions::VIEW_ATTACHING_CONTEST_PROBLEMS)) {
        return api_error403();
    }

    // We may read data several times (permission checking), so transaction is
    // used to ensure data consistency
    auto transaction = mysql.start_transaction();

    InplaceBuff<512> query;
    query.append(
        "SELECT c.id, c.name, r.id, r.name, p.id, p.name "
        "FROM contest_problems p "
        "STRAIGHT_JOIN contests c ON c.id=p.contest_id "
        "STRAIGHT_JOIN contest_rounds r ON r.id=p.contest_round_id "
        "WHERE p.problem_id=",
        problems_pid);

    enum ColumnIdx { CID, CNAME, CRID, CRNAME, CPID, CPNAME };

    // Process restrictions
    auto rows_limit = API_FIRST_QUERY_ROWS_LIMIT;
    StringView next_arg = url_args.extract_next_arg();
    for (bool id_condition_occurred = false; !next_arg.empty();
         next_arg = url_args.extract_next_arg())
    {
        auto arg = decode_uri(next_arg);
        char cond = arg[0];
        StringView arg_id = StringView{arg}.substr(1);

        if (not is_digit(arg_id)) {
            return api_error400();
        }

        // ID condition
        if (is_one_of(cond, '<', '>', '=')) {
            if (id_condition_occurred) {
                return api_error400("ID condition specified more than once");
            }

            rows_limit = API_OTHER_QUERY_ROWS_LIMIT;
            query.append(" AND p.id", arg);
            id_condition_occurred = true;

        } else {
            return api_error400();
        }
    }

    // Execute query
    query.append(" ORDER BY p.id DESC LIMIT ", rows_limit);
    auto res = mysql.query(query);

    // Column names
    // clang-format off
	append("[\n{\"columns\":["
	           "\"contest_id\","
	           "\"contest_name\","
	           "\"round_id\","
	           "\"round_name\","
	           "\"problem_id\","
	           "\"problem_name\""
	       "]}");
    // clang-format on

    while (res.next()) {
        append(
            ",\n[", res[CID], ",", json_stringify(res[CNAME]), ",", res[CRID], ",",
            json_stringify(res[CRNAME]), ",", res[CPID], ",", json_stringify(res[CPNAME]),
            "]");
    }

    append("\n]");
}

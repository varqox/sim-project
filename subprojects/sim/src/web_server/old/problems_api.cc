#include "sim.hh"

#include <cstdint>
#include <sim/jobs/utils.hh>
#include <sim/judging_config.hh>
#include <sim/problem_tags/old_problem_tag.hh>
#include <sim/problems/old_problem.hh>
#include <sim/problems/permissions.hh>
#include <sim/users/user.hh>
#include <simlib/config_file.hh>
#include <simlib/enum_val.hh>
#include <simlib/file_info.hh>
#include <simlib/file_manip.hh>
#include <simlib/from_unsafe.hh>
#include <simlib/humanize.hh>
#include <simlib/libzip.hh>
#include <simlib/sim/problem_package.hh>
#include <simlib/string_view.hh>
#include <type_traits>

using sim::jobs::OldJob;
using sim::problem_tags::OldProblemTag;
using sim::problems::OldProblem;
using sim::submissions::OldSubmission;
using sim::users::User;

namespace web_server::old {

static constexpr const char* proiblem_type_str(OldProblem::Type type) noexcept {
    using PT = OldProblem::Type;

    switch (type) {
    case PT::PUBLIC: return "Public";
    case PT::PRIVATE: return "Private";
    case PT::CONTEST_ONLY: return "Contest only";
    }
    return "Unknown";
}

void Sim::api_problems() {
    STACK_UNWINDING_MARK;

    using OPERMS = sim::problems::OverallPermissions;

    // We may read data several times (permission checking), so transaction is
    // used to ensure data consistency
    auto transaction = mysql.start_repeatable_read_transaction();

    InplaceBuff<512> qfields;
    InplaceBuff<512> qwhere;
    qfields.append("SELECT p.id, p.created_at, p.type, p.name, p.label, p.owner_id, "
                   "u.username, s.full_status");
    qwhere.append(
        " FROM problems p LEFT JOIN users u ON p.owner_id=u.id "
        "LEFT JOIN submissions s ON s.owner=",
        (session.has_value() ? StringView{from_unsafe{to_string(session->user_id)}} : "''"),
        " AND s.problem_id=p.id AND s.problem_final=1 "
        "WHERE TRUE"
    ); // Needed to easily append constraints

    enum ColumnIdx {
        PID,
        CREATED_AT,
        PTYPE,
        NAME,
        LABEL,
        OWNER_ID,
        OWN_USERNAME,
        SFULL_STATUS,
        SIMFILE
    };

    auto overall_perms = sim::problems::get_overall_permissions(
        session.has_value() ? std::optional{session->user_type} : std::nullopt
    );

    // Choose problems to select
    if (not uint(overall_perms & OPERMS::VIEW_ALL)) {
        if (not session.has_value()) {
            throw_assert(uint(overall_perms & OPERMS::VIEW_WITH_TYPE_PUBLIC));
            qwhere.append(" AND p.type=", EnumVal(OldProblem::Type::PUBLIC).to_int());

        } else if (session->user_type == User::Type::TEACHER) {
            throw_assert(
                uint(overall_perms & OPERMS::VIEW_WITH_TYPE_PUBLIC) and
                uint(overall_perms & OPERMS::VIEW_WITH_TYPE_CONTEST_ONLY)
            );
            qwhere.append(
                " AND (p.type=",
                EnumVal(OldProblem::Type::PUBLIC).to_int(),
                " OR p.type=",
                EnumVal(OldProblem::Type::CONTEST_ONLY).to_int(),
                " OR p.owner_id=",
                session->user_id,
                ')'
            );

        } else {
            throw_assert(uint(overall_perms & OPERMS::VIEW_WITH_TYPE_PUBLIC));
            qwhere.append(
                " AND (p.type=",
                EnumVal(OldProblem::Type::PUBLIC).to_int(),
                " OR p.owner_id=",
                session->user_id,
                ')'
            );
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
                qwhere.append(" AND p.type=", EnumVal(OldProblem::Type::PUBLIC).to_int());
            } else if (arg_id == "PRI") {
                qwhere.append(" AND p.type=", EnumVal(OldProblem::Type::PRIVATE).to_int());
            } else if (arg_id == "CON") {
                qwhere.append(" AND p.type=", EnumVal(OldProblem::Type::CONTEST_ONLY).to_int());
            } else {
                return api_error400(from_unsafe{concat("Invalid problem type: ", arg_id)});
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
            if (not session.has_value() or
                (str2num<decltype(session->user_id)>(arg_id) != session->user_id and
                 not uint(overall_perms & OPERMS::SELECT_BY_OWNER)))
            {
                return api_error403("You have no permissions to select others' "
                                    "problems by their user id");
            }

            qwhere.append(" AND p.owner_id=", arg_id);
            mask |= USER_ID_COND;

        } else {
            return api_error400();
        }
    }

    // Execute query
    qfields.append(qwhere, " ORDER BY p.id DESC LIMIT ", rows_limit);
    auto old_mysql = old_mysql::ConnectionView{mysql};
    auto res = old_mysql.query(qfields);

    // Column names
    // clang-format off
    append("[\n{\"columns\":["
               "\"id\","
               "\"created_at\","
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
    decltype(OldProblemTag::is_hidden) is_hidden;
    decltype(OldProblemTag::name) tag_name;
    auto stmt = old_mysql.prepare("SELECT name FROM problem_tags "
                                  "WHERE problem_id=? AND is_hidden=? ORDER BY name");
    stmt.bind_all(pid, is_hidden);
    stmt.res_bind_all(tag_name);

    while (res.next()) {
        EnumVal<OldProblem::Type> problem_type{
            WONT_THROW(str2num<OldProblem::Type::UnderlyingType>(res[PTYPE]).value())
        };
        auto problem_perms = sim::problems::get_permissions(
            (session.has_value() ? std::optional{session->user_id} : std::nullopt),
            (session.has_value() ? std::optional{session->user_type} : std::nullopt),
            (res.is_null(OWNER_ID)
                 ? std::nullopt
                 : decltype(OldProblem::owner_id)(WONT_THROW(
                       str2num<decltype(OldProblem::owner_id)::value_type>(res[OWNER_ID]).value()
                   ))),
            problem_type
        );
        using PERMS = sim::problems::Permissions;

        // Id
        append(",\n[", res[PID], ",");

        // Added
        if (uint(problem_perms & PERMS::VIEW_ADD_TIME)) {
            append("\"", res[CREATED_AT], "\",");
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
        if (uint(problem_perms & PERMS::VIEW_OWNER) and not res.is_null(OWNER_ID)) {
            append(res[OWNER_ID], ",\"", res[OWN_USERNAME], "\",");
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
            is_hidden = h;
            stmt.execute();
            if (stmt.next()) {
                for (;;) {
                    append(json_stringify(tag_name));
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
                css_color_class(EnumVal<OldSubmission::Status>(WONT_THROW(
                    str2num<OldSubmission::Status::UnderlyingType>(res[SFULL_STATUS]).value()
                ))),
                "\""
            );
        }

        // Append simfile and memory limit
        if (select_specified_problem and uint(problem_perms & PERMS::VIEW_SIMFILE)) {
            ConfigFile cf;
            cf.add_vars("memory_limit");
            cf.load_config_from_string(res[SIMFILE].to_string());
            append(
                ',',
                json_stringify(res[SIMFILE]), // simfile
                ',',
                json_stringify(cf.get_var("memory_limit").as_string())
            );
        }

        append(']');
    }

    append("\n]");
}

void Sim::api_problem() {
    STACK_UNWINDING_MARK;

    StringView next_arg = url_args.extract_next_arg();
    if (not is_digit(next_arg)) {
        return api_error400();
    }

    problems_pid = next_arg;

    old_mysql::Optional<decltype(OldProblem::owner_id)::value_type> problem_owner_id;
    decltype(OldProblem::label) problem_label;
    decltype(OldProblem::simfile) problem_simfile;
    decltype(OldProblem::type) problem_type;

    auto old_mysql = old_mysql::ConnectionView{mysql};
    auto stmt = old_mysql.prepare("SELECT file_id, owner_id, type, label, simfile "
                                  "FROM problems WHERE id=?");
    stmt.bind_and_execute(problems_pid);
    stmt.res_bind_all(
        problems_file_id, problem_owner_id, problem_type, problem_label, problem_simfile
    );
    if (not stmt.next()) {
        return api_error404();
    }

    auto problem_perms = sim::problems::get_permissions(
        (session.has_value() ? std::optional{session->user_id} : std::nullopt),
        (session.has_value() ? std::optional{session->user_type} : std::nullopt),
        problem_owner_id,
        problem_type
    );

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

void Sim::api_statement_impl(
    uint64_t problem_file_id, StringView problem_label, StringView simfile
) {
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

    resp.headers["Content-Disposition"] =
        concat_tostr("inline; filename=", ::http::quote(from_unsafe{concat(problem_label, ext)}));

    // TODO: maybe add some cache system for the statements?
    ZipFile zip(sim::internal_files::old_path_of(problem_file_id), ZIP_RDONLY);
    resp.content =
        zip.extract_to_str(zip.get_index(concat(sim::zip_package_main_dir(zip), statement)));
}

void Sim::api_problem_statement(
    StringView problem_label, StringView simfile, sim::problems::Permissions perms
) {
    STACK_UNWINDING_MARK;

    if (uint(~perms & sim::problems::Permissions::VIEW_STATEMENT)) {
        return api_error403();
    }

    return api_statement_impl(problems_file_id, problem_label, simfile);
}

void Sim::api_problem_download(StringView problem_label, sim::problems::Permissions perms) {
    STACK_UNWINDING_MARK;

    if (uint(~perms & sim::problems::Permissions::DOWNLOAD)) {
        return api_error403();
    }

    resp.headers["Content-Disposition"] =
        concat_tostr("attachment; filename=", problem_label, ".zip");
    resp.content_type = http::Response::FILE;
    resp.content = sim::internal_files::old_path_of(problems_file_id);
}

void Sim::api_problem_rejudge_all_submissions(sim::problems::Permissions perms) {
    STACK_UNWINDING_MARK;

    if (uint(~perms & sim::problems::Permissions::REJUDGE_ALL)) {
        return api_error403();
    }

    auto old_mysql = old_mysql::ConnectionView{mysql};
    old_mysql
        .prepare("INSERT jobs (creator, status, priority, type, created_at, aux_id,"
                 " info, data) "
                 "SELECT ?, ?, ?, ?, ?, id, '', '' "
                 "FROM submissions WHERE problem_id=? ORDER BY id")
        .bind_and_execute(
            session->user_id,
            EnumVal(OldJob::Status::PENDING),
            default_priority(OldJob::Type::REJUDGE_SUBMISSION),
            EnumVal(OldJob::Type::REJUDGE_SUBMISSION),
            mysql_date(),
            problems_pid
        );

    sim::jobs::notify_job_server();
}

void Sim::api_problem_reset_time_limits(sim::problems::Permissions perms) {
    STACK_UNWINDING_MARK;

    if (uint(~perms & sim::problems::Permissions::RESET_TIME_LIMITS)) {
        return api_error403();
    }

    auto old_mysql = old_mysql::ConnectionView{mysql};
    old_mysql
        .prepare("INSERT jobs (creator, status, priority, type, created_at, aux_id,"
                 " info, data) "
                 "VALUES(?, ?, ?, ?, ?, ?, '', '')")
        .bind_and_execute(
            session->user_id,
            EnumVal(OldJob::Status::PENDING),
            default_priority(OldJob::Type::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION),
            EnumVal(OldJob::Type::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION),
            mysql_date(),
            problems_pid
        );

    sim::jobs::notify_job_server();
    append(old_mysql.insert_id());
}

void Sim::api_problem_delete(sim::problems::Permissions perms) {
    STACK_UNWINDING_MARK;

    if (uint(~perms & sim::problems::Permissions::DELETE)) {
        return api_error403();
    }

    if (not check_submitted_password()) {
        return api_error403("Invalid password");
    }

    // Queue deleting job

    auto old_mysql = old_mysql::ConnectionView{mysql};
    old_mysql
        .prepare("INSERT jobs (creator, status, priority, type, created_at, aux_id,"
                 " info, data) "
                 "VALUES(?, ?, ?, ?, ?, ?, '', '')")
        .bind_and_execute(
            session->user_id,
            EnumVal(OldJob::Status::PENDING),
            default_priority(OldJob::Type::DELETE_PROBLEM),
            EnumVal(OldJob::Type::DELETE_PROBLEM),
            mysql_date(),
            problems_pid
        );

    sim::jobs::notify_job_server();
    append(old_mysql.insert_id());
}

void Sim::api_problem_merge_into_another(sim::problems::Permissions perms) {
    STACK_UNWINDING_MARK;

    if (uint(~perms & sim::problems::Permissions::MERGE)) {
        return api_error403();
    }

    InplaceBuff<32> target_problem_id;
    bool rejudge_transferred_submissions =
        request.form_fields.contains("rejudge_transferred_submissions");
    form_validate_not_blank(target_problem_id, "target_problem", "Target problem ID", is_digit_not_greater_than<std::numeric_limits<decltype(sim::jobs::MergeProblemsInfo::target_problem_id)>::max()>);

    if (notifications.size) {
        return api_error400(notifications);
    }

    if (problems_pid == target_problem_id) {
        return api_error400("You cannot merge problem with itself");
    }

    auto tp_perms_opt = sim::problems::get_permissions(
        mysql,
        target_problem_id,
        (session.has_value() ? std::optional{session->user_id} : std::nullopt),
        (session.has_value() ? std::optional{session->user_type} : std::nullopt)
    );
    if (not tp_perms_opt) {
        return api_error400("Target problem does not exist");
    }

    auto tp_perms = tp_perms_opt.value();
    if (uint(~tp_perms & sim::problems::Permissions::MERGE)) {
        return api_error403("You do not have permission to merge to the target problem");
    }

    if (not check_submitted_password()) {
        return api_error403("Invalid password");
    }

    // Queue merging job
    auto old_mysql = old_mysql::ConnectionView{mysql};
    old_mysql
        .prepare("INSERT jobs (creator, status, priority, type, created_at, aux_id,"
                 " info, data) "
                 "VALUES(?, ?, ?, ?, ?, ?, ?, '')")
        .bind_and_execute(
            session->user_id,
            EnumVal(OldJob::Status::PENDING),
            default_priority(OldJob::Type::MERGE_PROBLEMS),
            EnumVal(OldJob::Type::MERGE_PROBLEMS),
            mysql_date(),
            problems_pid,
            sim::jobs::MergeProblemsInfo(
                WONT_THROW(str2num<decltype(sim::jobs::MergeProblemsInfo::target_problem_id)>(
                               target_problem_id
                )
                               .value()),
                rejudge_transferred_submissions
            )
                .dump()
        );

    sim::jobs::notify_job_server();
    append(old_mysql.insert_id());
}

void Sim::api_problem_edit(sim::problems::Permissions perms) {
    STACK_UNWINDING_MARK;

    StringView next_arg = url_args.extract_next_arg();
    if (next_arg == "tags") {
        return api_problem_edit_tags(perms);
    }

    return api_error400();
}

void Sim::api_problem_edit_tags(sim::problems::Permissions perms) {
    STACK_UNWINDING_MARK;

    auto add_tag = [&] {
        bool hidden = (request.form_fields.get("hidden") == "true");
        StringView name;
        form_validate_not_blank(name, "name", "Tag name", decltype(OldProblemTag::name)::max_len);
        if (notifications.size) {
            return api_error400(notifications);
        }

        if (uint(
                ~perms &
                (hidden ? sim::problems::Permissions::EDIT_HIDDEN_TAGS
                        : sim::problems::Permissions::EDIT_TAGS)
            ))
        {
            return api_error403();
        }

        auto old_mysql = old_mysql::ConnectionView{mysql};
        auto stmt = old_mysql.prepare("INSERT IGNORE "
                                      "INTO problem_tags(problem_id, name, is_hidden) "
                                      "VALUES(?, ?, ?)");
        stmt.bind_and_execute(problems_pid, name, hidden);

        if (stmt.affected_rows() == 0) {
            return api_error400("Tag already exist");
        }
    };

    auto edit_tag = [&] {
        bool is_hidden = (request.form_fields.get("hidden") == "true");
        StringView name;
        StringView old_name;
        form_validate_not_blank(name, "name", "Tag name", decltype(OldProblemTag::name)::max_len);
        form_validate_not_blank(
            old_name, "old_name", "Old tag name", decltype(OldProblemTag::name)::max_len
        );
        if (notifications.size) {
            return api_error400(notifications);
        }

        if (uint(
                ~perms &
                (is_hidden ? sim::problems::Permissions::EDIT_HIDDEN_TAGS
                           : sim::problems::Permissions::EDIT_TAGS)
            ))
        {
            return api_error403();
        }

        if (name == old_name) {
            return;
        }

        auto transaction = mysql.start_repeatable_read_transaction();

        auto old_mysql = old_mysql::ConnectionView{mysql};
        auto stmt = old_mysql.prepare("SELECT 1 FROM problem_tags "
                                      "WHERE name=? AND problem_id=? AND is_hidden=?");
        stmt.bind_and_execute(name, problems_pid, is_hidden);
        if (stmt.next()) {
            return api_error400("Tag already exist");
        }

        stmt = old_mysql.prepare("UPDATE problem_tags SET name=? "
                                 "WHERE problem_id=? AND name=? AND is_hidden=?");
        stmt.bind_and_execute(name, problems_pid, old_name, is_hidden);
        if (stmt.affected_rows() == 0) {
            return api_error400("Tag does not exist");
        }

        transaction.commit();
    };

    auto delete_tag = [&] {
        StringView name;
        bool is_hidden = (request.form_fields.get("hidden") == "true");
        form_validate_not_blank(name, "name", "Tag name", decltype(OldProblemTag::name)::max_len);
        if (notifications.size) {
            return api_error400(notifications);
        }

        if (uint(
                ~perms &
                (is_hidden ? sim::problems::Permissions::EDIT_HIDDEN_TAGS
                           : sim::problems::Permissions::EDIT_TAGS)
            ))
        {
            return api_error403();
        }

        auto old_mysql = old_mysql::ConnectionView{mysql};
        old_mysql
            .prepare("DELETE FROM problem_tags "
                     "WHERE problem_id=? AND name=? AND is_hidden=?")
            .bind_and_execute(problems_pid, name, is_hidden);
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

void Sim::api_problem_change_statement(sim::problems::Permissions perms) {
    STACK_UNWINDING_MARK;

    if (uint(~perms & sim::problems::Permissions::CHANGE_STATEMENT)) {
        return api_error403();
    }

    CStringView statement_path;
    CStringView statement_file;
    form_validate(statement_path, "path", "New statement path");
    form_validate_file_path_not_blank(statement_file, "statement", "New statement");

    if (get_file_size(statement_file) > OldProblem::new_statement_max_size) {
        add_notification(
            "error",
            "New statement file is too big (maximum allowed size: ",
            OldProblem::new_statement_max_size,
            " bytes = ",
            humanize_file_size(OldProblem::new_statement_max_size),
            ')'
        );
    }

    if (notifications.size) {
        return api_error400(notifications);
    }

    sim::jobs::ChangeProblemStatementInfo cps_info(statement_path);

    auto transaction = mysql.start_repeatable_read_transaction();

    auto old_mysql = old_mysql::ConnectionView{mysql};
    old_mysql.prepare("INSERT INTO internal_files (created_at) VALUES(?)")
        .bind_and_execute(mysql_date());
    auto job_file_id = old_mysql.insert_id();
    FileRemover job_file_remover(sim::internal_files::old_path_of(job_file_id).to_string());

    // Make uploaded statement file the job's file
    if (move(statement_file, sim::internal_files::old_path_of(job_file_id))) {
        THROW("move()", errmsg());
    }

    old_mysql
        .prepare("INSERT jobs (file_id, creator, status, priority, type,"
                 " created_at, aux_id, info, data) "
                 "VALUES(?, ?, ?, ?, ?, ?, ?, ?, '')")
        .bind_and_execute(
            job_file_id,
            session->user_id,
            EnumVal(OldJob::Status::PENDING),
            default_priority(OldJob::Type::CHANGE_PROBLEM_STATEMENT),
            EnumVal(OldJob::Type::CHANGE_PROBLEM_STATEMENT),
            mysql_date(),
            problems_pid,
            cps_info.dump()
        );

    auto job_id = old_mysql.insert_id(); // Has to be retrieved before commit

    transaction.commit();
    job_file_remover.cancel();
    sim::jobs::notify_job_server();

    append(job_id);
}

void Sim::api_problem_attaching_contest_problems(sim::problems::Permissions perms) {
    STACK_UNWINDING_MARK;

    if (uint(~perms & sim::problems::Permissions::VIEW_ATTACHING_CONTEST_PROBLEMS)) {
        return api_error403();
    }

    // We may read data several times (permission checking), so transaction is
    // used to ensure data consistency
    auto transaction = mysql.start_repeatable_read_transaction();

    InplaceBuff<512> query;
    query.append(
        "SELECT c.id, c.name, r.id, r.name, p.id, p.name "
        "FROM contest_problems p "
        "STRAIGHT_JOIN contests c ON c.id=p.contest_id "
        "STRAIGHT_JOIN contest_rounds r ON r.id=p.contest_round_id "
        "WHERE p.problem_id=",
        problems_pid
    );

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

    auto old_mysql = old_mysql::ConnectionView{mysql};
    auto res = old_mysql.query(query);

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
            ",\n[",
            res[CID],
            ",",
            json_stringify(res[CNAME]),
            ",",
            res[CRID],
            ",",
            json_stringify(res[CRNAME]),
            ",",
            res[CPID],
            ",",
            json_stringify(res[CPNAME]),
            "]"
        );
    }

    append("\n]");
}

} // namespace web_server::old

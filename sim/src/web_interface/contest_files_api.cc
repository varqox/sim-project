#include "sim/constants.hh"
#include "sim/jobs.hh"
#include "sim/random.hh"
#include "simlib/call_in_destructor.hh"
#include "simlib/file_info.hh"
#include "simlib/file_manip.hh"
#include "simlib/humanize.hh"
#include "src/web_interface/sim.hh"

void Sim::api_contest_files() {
    STACK_UNWINDING_MARK;

    using sim::contest_file::OverallPermissions;
    using sim::contest_file::Permissions;

    // We may read data several times (permission checking), so transaction is
    // used to ensure data consistency
    auto transaction = mysql.start_transaction();

    bool allow_access = false; // Either contest or specific id condition must occur

    InplaceBuff<512> query;
    query.append("SELECT cf.id, cf.name, cf.description, cf.file_size,"
                 " cf.modified, cf.contest_id, cf.creator, u.username "
                 "FROM contest_files cf "
                 "LEFT JOIN users u ON u.id=cf.creator "
                 "WHERE TRUE"); // Needed to easily append constraints

    enum ColumnIdx {
        FID,
        FNAME,
        DESCRIPTION,
        FSIZE,
        MODIFIED,
        CONTEST_ID,
        CREATOR,
        CUSERNAME
    };

    auto append_column_names = [&] {
        // clang-format off
        append("[{\"fields\":["
                   "\"overall_actions\","
                   "{\"name\":\"rows\", \"columns\":["
                       "\"id\","
                       "\"name\","
                       "\"description\","
                       "\"file_size\","
                       "\"modified\","
                       "\"contest_id\","
                       "\"creator_id\","
                       "\"creator_username\","
                       "\"actions\""
                   "]}"
               "]}");
        // clang-format on
    };

    auto set_empty_response = [&] {
        resp.content.clear();
        append_column_names();
        append(",\n\"\",[\n]]"); // Empty overall actions
    };

    // Precess restrictions
    auto rows_limit = API_FIRST_QUERY_ROWS_LIMIT;
    Permissions perms = Permissions::NONE;
    OverallPermissions overall_perms = OverallPermissions::NONE;
    {
        bool id_condition_occurred = false;
        bool contest_id_condition_occurred = false;

        for (StringView next_arg = url_args.extract_next_arg(); not next_arg.empty();
             next_arg = url_args.extract_next_arg())
        {
            auto arg = decode_uri(next_arg);
            char cond_c = arg[0];
            StringView arg_id = StringView(arg).substr(1);

            // Contest file ID
            if (is_one_of(cond_c, '<', '>', '=')) {
                if (not is_alnum(arg_id)) {
                    return api_error400("Invalid Contest file ID");
                }

                if (id_condition_occurred) {
                    return api_error400("Contest file ID condition specified more than once");
                }

                rows_limit = API_OTHER_QUERY_ROWS_LIMIT;
                id_condition_occurred = true;

                if (cond_c == '=' and not allow_access) {
                    auto perms_opt = sim::contest_file::get_permissions(
                        mysql, arg_id,
                        (session_is_open ? std::optional{session_user_id} : std::nullopt));
                    if (perms_opt) {
                        std::tie(perms, overall_perms) = perms_opt.value();
                    }

                    allow_access = uint(perms & Permissions::VIEW);
                }

                query.append(" AND cf.id", cond_c, '\'', arg_id, '\'');

                // NOLINTNEXTLINE(bugprone-branch-clone)
            } else if (not is_digit(arg_id)) { // Next conditions require arg_id
                                               // to be a valid (digital) ID
                return api_error400();

            } else if (cond_c == 'c') { // Contest id
                if (contest_id_condition_occurred) {
                    return api_error400("Contest ID condition specified more than once");
                }

                contest_id_condition_occurred = true;
                query.append(" AND cf.contest_id=", arg_id);

                auto cperms = sim::contest::get_permissions(
                    mysql, arg_id,
                    (session_is_open ? std::optional{session_user_id} : std::nullopt));
                if (not cperms) {
                    return set_empty_response(); // Do allow to query for
                                                 // contest existence (not
                                                 // api_error404())
                }

                perms = sim::contest_file::get_permissions(cperms.value());
                overall_perms = sim::contest_file::get_overall_permissions(cperms.value());
                if (uint(perms & Permissions::VIEW)) {
                    allow_access = true;
                }

            } else {
                return api_error400();
            }
        }
    }

    if (not allow_access) {
        return set_empty_response();
    }

    // Execute query
    auto res = mysql.query(intentional_unsafe_string_view(
        concat(query, " ORDER BY cf.id DESC LIMIT ", rows_limit)));

    append_column_names();

    // Overall actions
    append(",\n\"");
    if (uint(overall_perms & OverallPermissions::ADD)) {
        append('A');
    }
    if (uint(overall_perms & OverallPermissions::VIEW_CREATOR)) {
        append('C');
    }
    append("\",[");

    for (bool first = true; res.next();) {
        if (first) {
            first = false;
        } else {
            append(',');
        }

        append(
            "\n[", json_stringify(res[FID]), ',', json_stringify(res[FNAME]), ',',
            json_stringify(res[DESCRIPTION]), ',', res[FSIZE], ",\"", res[MODIFIED], "\",",
            res[CONTEST_ID], ',');

        // Permissions are already loaded
        if (uint(overall_perms & OverallPermissions::VIEW_CREATOR) and
            not res.is_null(CREATOR)) {
            append(res[CREATOR], ',', json_stringify(res[CUSERNAME]), ',');
        } else {
            append("null,null,");
        }

        // Actions
        append("\"");
        if (uint(perms & Permissions::DOWNLOAD)) {
            append("O");
        }
        if (uint(perms & Permissions::EDIT)) {
            append("E");
        }
        if (uint(perms & Permissions::DELETE)) {
            append("D");
        }
        append("\"]");
    }

    append("\n]]");
}

void Sim::api_contest_file() {
    STACK_UNWINDING_MARK;

    StringView contest_file_id = url_args.extract_next_arg();
    if (contest_file_id == "add") {
        return api_contest_file_add();
    }
    if (contest_file_id.empty()) {
        return api_error404();
    }

    sim::contest_file::Permissions perms;
    {
        auto perms_opt = sim::contest_file::get_permissions(
            mysql, contest_file_id,
            (session_is_open ? std::optional{session_user_id} : std::nullopt));
        if (not perms_opt) {
            return api_error404();
        }

        perms = perms_opt.value().first;
    }

    StringView next_arg = url_args.extract_next_arg();
    if (next_arg == "download") {
        return api_contest_file_download(contest_file_id, perms);
    }
    if (next_arg == "edit") {
        return api_contest_file_edit(contest_file_id, perms);
    }
    if (next_arg == "delete") {
        return api_contest_file_delete(contest_file_id, perms);
    }
    if (not next_arg.empty()) {
        return api_error400();
    }
}

void Sim::api_contest_file_download(
    StringView contest_file_id, sim::contest_file::Permissions perms) {
    if (uint(~perms & sim::contest_file::Permissions::DOWNLOAD)) {
        return api_error403();
    }

    uint64_t internal_file_id = 0;
    InplaceBuff<FILE_NAME_MAX_LEN> filename;
    auto stmt = mysql.prepare("SELECT file_id, name FROM contest_files "
                              "WHERE id=?");
    stmt.bind_and_execute(contest_file_id);
    stmt.res_bind_all(internal_file_id, filename);
    if (not stmt.next()) {
        return error404(); // Should not happen as the file existed a moment ago
    }

    resp.headers["Content-Disposition"] =
        concat_tostr("attachment; filename=", http::quote(filename));
    resp.content_type = server::HttpResponse::FILE;
    resp.content = internal_file_path(internal_file_id);
}

void Sim::api_contest_file_add() {
    StringView contest_id = url_args.extract_next_arg();
    if (contest_id.empty() or contest_id[0] != 'c' or
        not is_digit(contest_id = contest_id.substr(1)))
    {
        return api_error400();
    }

    auto cperms = sim::contest::get_permissions(
        mysql, contest_id, (session_is_open ? std::optional{session_user_id} : std::nullopt));
    if (not cperms) {
        return api_error404(); // TODO: maybe too much information?
    }

    using sim::contest_file::OverallPermissions;
    OverallPermissions overall_perms =
        sim::contest_file::get_overall_permissions(cperms.value());
    if (uint(~overall_perms & OverallPermissions::ADD)) {
        return api_error403();
    }

    CStringView name;
    CStringView description;
    CStringView file_tmp_path;
    CStringView user_filename;
    form_validate(name, "name", "File name", FILE_NAME_MAX_LEN);
    form_validate(description, "description", "Description", FILE_DESCRIPTION_MAX_LEN);
    bool file_exists = form_validate_file_path_not_blank(file_tmp_path, "file", "File");
    form_validate_not_blank(user_filename, "file", "Filename of the selected file");

    if (name.empty()) {
        if (user_filename.size() > FILE_NAME_MAX_LEN) {
            add_notification(
                "error", "Filename of the provided file is longer than ", FILE_NAME_MAX_LEN,
                ". You have to provide a shorter name.");
        } else {
            name = user_filename;
        }
    }

    // Check the file size
    auto file_size = (file_exists ? get_file_size(file_tmp_path) : 0);
    if (file_size > FILE_MAX_SIZE) {
        add_notification(
            "error", "File is too big (maximum allowed size: ", FILE_MAX_SIZE,
            " bytes = ", humanize_file_size(FILE_MAX_SIZE), ')');
        return api_error400(notifications);
    }

    if (notifications.size) {
        return api_error400(notifications);
    }

    auto transaction = mysql.start_transaction();

    mysql.update("INSERT INTO internal_files VALUES()");
    auto internal_file_id = mysql.insert_id();

    CallInDtor internal_file_remover(
        [internal_file_id] { (void)unlink(internal_file_path(internal_file_id)); });

    // Insert file
    auto stmt = mysql.prepare("INSERT IGNORE contest_files (id, file_id,"
                              " contest_id, name, description, file_size,"
                              " modified, creator) "
                              "VALUES(?,?,?,?,?,?,?,?)");

    InplaceBuff<FILE_ID_LEN> file_id;
    auto curr_date = mysql_date();
    throw_assert(session_is_open);

    do {
        file_id = generate_random_token(FILE_ID_LEN);
        stmt.bind_and_execute(
            file_id, internal_file_id, contest_id, name, description, file_size, curr_date,
            session_user_id);
    } while (stmt.affected_rows() == 0);

    // Move file
    if (move(file_tmp_path, internal_file_path(internal_file_id))) {
        THROW("move()", errmsg());
    }

    transaction.commit();
    internal_file_remover.cancel();

    append(file_id);
}

void Sim::api_contest_file_edit(
    StringView contest_file_id, sim::contest_file::Permissions perms) {
    if (uint(~perms & sim::contest_file::Permissions::EDIT)) {
        return api_error403();
    }

    CStringView name;
    CStringView description;
    CStringView file_tmp_path;
    form_validate(name, "name", "File name", FILE_NAME_MAX_LEN);
    form_validate(description, "description", "Description", FILE_DESCRIPTION_MAX_LEN);
    bool reuploading_file = form_validate_file_path_not_blank(file_tmp_path, "file", "File");

    CStringView user_filename = request.form_fields.get_or("file", "");
    reuploading_file &= not user_filename.empty();

    if (name.empty()) {
        if (user_filename.size() > FILE_NAME_MAX_LEN) {
            add_notification(
                "error", "Filename of the provided file is longer than ", FILE_NAME_MAX_LEN,
                ". You have to provide a shorter name.");
        } else if (user_filename.empty()) {
            add_notification(
                "error",
                "File cannot have an empty name - no name and "
                "file (to infer name) has been provided");
        } else {
            name = user_filename;
        }
    }

    // Check the file size
    auto file_size = (reuploading_file ? get_file_size(file_tmp_path) : 0);
    if (file_size > FILE_MAX_SIZE) {
        add_notification(
            "error", "File is too big (maximum allowed size: ", FILE_MAX_SIZE,
            " bytes = ", humanize_file_size(FILE_MAX_SIZE), ')');
        return api_error400(notifications);
    }

    if (notifications.size) {
        return api_error400(notifications);
    }

    auto transaction = mysql.start_transaction();

    uint64_t internal_file_id = 0;
    CallInDtor internal_file_remover([internal_file_id] {
        if (internal_file_id > 0) {
            (void)unlink(internal_file_path(internal_file_id));
        }
    });

    if (reuploading_file) {
        mysql.update("INSERT INTO internal_files VALUES()");
        internal_file_id = mysql.insert_id();

        // Move file
        if (move(file_tmp_path, internal_file_path(internal_file_id))) {
            THROW("move()", errmsg());
        }

        mysql
            .prepare("INSERT INTO jobs(file_id, creator, type, priority,"
                     " status, added, aux_id, info, data) "
                     "SELECT file_id, NULL, ?, ?, ?, ?, NULL, '', '' "
                     "FROM contest_files WHERE id=?")
            .bind_and_execute(
                EnumVal(JobType::DELETE_FILE), priority(JobType::DELETE_FILE),
                EnumVal(JobStatus::PENDING), mysql_date(), contest_file_id);
    }

    // Update file
    auto stmt = mysql.prepare("UPDATE contest_files "
                              "SET file_id=IF(?, ?, file_id), name=?,"
                              " description=?, file_size=IF(?, ?, file_size),"
                              " modified=? "
                              "WHERE id=?");
    stmt.bind_and_execute(
        reuploading_file, internal_file_id, name, description, reuploading_file, file_size,
        mysql_date(), contest_file_id);

    transaction.commit();
    internal_file_remover.cancel();
    if (reuploading_file) {
        jobs::notify_job_server();
    }
}

void Sim::api_contest_file_delete(
    StringView contest_file_id, sim::contest_file::Permissions perms) {
    if (uint(~perms & sim::contest_file::Permissions::DELETE)) {
        return api_error403();
    }

    auto transaction = mysql.start_transaction();

    mysql
        .prepare("INSERT INTO jobs(file_id, creator, type, priority, status,"
                 " added, aux_id, info, data) "
                 "SELECT file_id, NULL, ?, ?, ?, ?, NULL, '', '' "
                 "FROM contest_files WHERE id=?")
        .bind_and_execute(
            EnumVal(JobType::DELETE_FILE), priority(JobType::DELETE_FILE),
            EnumVal(JobStatus::PENDING), mysql_date(), contest_file_id);

    mysql.prepare("DELETE FROM contest_files WHERE id=?").bind_and_execute(contest_file_id);

    transaction.commit();
    jobs::notify_job_server();
}

#include "contest.h"
#include "form_validator.h"

#include <sim/constants.h>
#include <simlib/debug.h>
#include <simlib/process.h>
#include <simlib/time.h>

using std::string;

void Contest::addFile() {
	if (!rpath->admin_access)
		return error403();

	FormValidator fv(req->form_data);
	string file_name, description;
	if (req->method == server::HttpRequest::POST) {
		string user_file_name;
		// Validate all fields
		fv.validate(file_name, "file-name", "File name", FILE_NAME_MAX_LEN);

		fv.validateNotBlank(user_file_name, "file", "File");

		fv.validate(description, "description", "Description",
			FILE_DESCRIPTION_MAX_LEN);

		if (file_name.empty())
			file_name = user_file_name;

		// If all fields are OK
		if (fv.noErrors())
			try {
				string id;
				string current_time = date("%Y-%m-%d %H:%M:%S");
				// Insert file to `files`
				DB::Statement stmt = db_conn.prepare("INSERT IGNORE files "
						"(id, round_id, name, description, modified) "
						"VALUES(?,NULL,?,?,?)");
				stmt.setString(2, file_name);
				stmt.setString(3, description);
				stmt.setString(4, current_time);
				do {
					id = generateId(FILE_ID_LEN);
					stmt.setString(1, id);
				} while (stmt.executeUpdate() == 0);

				// Move file
				SignalBlocker signal_guard;
				if (move(fv.getFilePath("file"), concat("files/", id)))
					THROW("move()", error(errno));

				stmt = db_conn.prepare(
					"UPDATE files SET round_id=? WHERE id=?");
				stmt.setString(1, rpath->round_id);
				stmt.setString(2, id);

				if (stmt.executeUpdate() != 1) {
					(void)remove(concat("files/", id));
					THROW("Failed to update inserted file");
				}
				signal_guard.unblock();

				return redirect(concat("/c/", rpath->round_id, "/files"));

			} catch (const std::exception& e) {
				fv.addError("Internal server error");
				ERRLOG_CAUGHT(e);
			}
	}

	auto ender = contestTemplate("Add file");
	printRoundPath();
	append(fv.errors(), "<div class=\"form-container\">\n"
			"<h1>Add file</h1>"
			"<form method=\"post\" enctype=\"multipart/form-data\">\n"
				// File name
				"<div class=\"field-group\">\n"
					"<label>File name</label>\n"
					"<input type=\"text\" name=\"file-name\" value=\"",
						htmlSpecialChars(file_name), "\" size=\"24\" "
						"maxlength=\"", toStr(FILE_NAME_MAX_LEN), "\" "
						"placeholder=\"The same as name of uploaded file\">\n"
				"</div>\n"
				// File
				"<div class=\"field-group\">\n"
					"<label>File</label>\n"
					"<input type=\"file\" name=\"file\" required>\n"
				"</div>\n"
				// Description
				"<div class=\"field-group\">\n"
					"<label>Description</label>\n"
					"<textarea name=\"description\" maxlength=\"",
						toStr(FILE_DESCRIPTION_MAX_LEN), "\">",
						htmlSpecialChars(description), "</textarea>"
				"</div>\n"

				"<div class=\"button-row\">\n"
					"<input class=\"btn blue\" type=\"submit\" value=\"Submit\">\n"
					"<a class=\"btn\" href=\"/c/", rpath->round_id ,"/files\">"
						"Go back</a>"
				"</div>\n"
			"</form>\n"
		"</div>\n");
}

void Contest::editFile(const StringView& id, string name) {
	if (!rpath->admin_access)
		return error403();

	FormValidator fv(req->form_data);
	string modified, description;
	if (req->method == server::HttpRequest::POST) {
		// Validate all fields
		fv.validate(name, "file-name", "File name", FILE_NAME_MAX_LEN);

		fv.validate(description, "description", "Description",
			FILE_DESCRIPTION_MAX_LEN);

		if (name.empty()) {
			name = fv.get("file");
			if (name.empty())
				fv.addError(
					"File name cannot be blank unless you upload a new file");
		}

		// If all fields are OK
		if (fv.noErrors())
			try {
				string current_time = date("%Y-%m-%d %H:%M:%S");
				DB::Statement stmt = db_conn.prepare("UPDATE files "
						"SET name=?, description=?, modified=? WHERE id=?");
				stmt.setString(1, name);
				stmt.setString(2, description);
				stmt.setString(3, current_time);
				stmt.setString(4, id.to_string());
				stmt.executeUpdate();

				// Move file
				if (fv.get("file").size()) {
					SignalBlocker signal_guard;
					if (move(fv.getFilePath("file"), concat("files/", id)))
						THROW("move()", error(errno));
				}

				fv.addError("Update successful");

			} catch (const std::exception& e) {
				fv.addError("Internal server error");
				ERRLOG_CAUGHT(e);
			}
	}

	try {
		DB::Statement stmt = db_conn.prepare(
			"SELECT name, description, modified FROM files WHERE id=?");
		stmt.setString(1, id.to_string());

		DB::Result res = stmt.executeQuery();
		if (!res.next())
			return error404();

		name = res[1];
		description = res[2];
		modified = res[3];

	} catch (const std::exception& e) {
		fv.addError("Internal server error");
		ERRLOG_CAUGHT(e);
	}

	auto ender = contestTemplate("Edit file");
	printRoundPath();
	append(fv.errors(), "<div class=\"form-container\">\n"
			"<h1>Edit file</h1>"
			"<form method=\"post\" enctype=\"multipart/form-data\">\n"
				// File name
				"<div class=\"field-group\">\n"
					"<label>File name</label>\n"
					"<input type=\"text\" name=\"file-name\" value=\"",
						htmlSpecialChars(name), "\" size=\"24\" "
						"maxlength=\"", toStr(FILE_NAME_MAX_LEN), "\" "
						"placeholder=\"The same as name of reuploaded file\">\n"
				"</div>\n"
				// Reupload file
				"<div class=\"field-group\">\n"
					"<label>Reupload file</label>\n"
					"<input type=\"file\" name=\"file\">\n"
				"</div>\n"
				// Description
				"<div class=\"field-group\">\n"
					"<label>Description</label>\n"
					"<textarea name=\"description\" maxlength=\"",
						toStr(FILE_DESCRIPTION_MAX_LEN), "\">",
						htmlSpecialChars(description), "</textarea>"
				"</div>\n"
				// Modified
				"<div class=\"field-group\">\n"
					"<label>Modified</label>\n"
					"<input type=\"text\" value=\"", modified, "\" disabled>\n"
				"</div>\n"

				"<div class=\"button-row\">\n"
					"<input class=\"btn blue\" type=\"submit\" value=\"Update\">\n"
					"<a class=\"btn\" href=\"/c/", rpath->round_id ,"/files\">"
						"Go back</a>"
					"<a class=\"btn red\" href=\"/file/", id, "/delete\">"
						"Delete file</a>\n"
				"</div>\n"
			"</form>\n"
		"</div>\n");
}

void Contest::deleteFile(const StringView& id, const StringView& name) {
	if (!rpath->admin_access)
		return error403();

	string referer = req->headers.get("Referer");
	if (referer.empty())
		referer = concat("/c/", rpath->round_id, "/files");

	FormValidator fv(req->form_data);
	if (req->method == server::HttpRequest::POST && fv.exist("delete"))
		try {
			DB::Statement stmt = db_conn.prepare(
				"DELETE FROM files WHERE id=?");
			stmt.setString(1, id.to_string());

			if (stmt.executeUpdate() != 1)
				return error500();

			if (BLOCK_SIGNALS(remove(concat("files/", id))))
				THROW("remove()", error(errno));

			return redirect(referer);

		} catch (const std::exception& e) {
			fv.addError("Internal server error");
			ERRLOG_CAUGHT(e);
		}

	auto ender = contestTemplate("Delete file");
	printRoundPath();
	append(fv.errors(), "<div class=\"form-container\">\n"
		"<h1>Delete file</h1>\n"
		"<form method=\"post\">\n"
			"<label class=\"field\">Are you sure to delete file "
				"<a href=\"/file/", id, "/edit\">",
					htmlSpecialChars(name), "</a>?"
			"</label>\n"
			"<div class=\"submit-yes-no\">\n"
				"<button class=\"btn red\" type=\"submit\" name=\"delete\">"
					"Yes, I'm sure</button>\n"
				"<a class=\"btn\" href=\"", referer, "\">No, go back</a>\n"
			"</div>\n"
		"</form>\n"
	"</div>\n");
}

void Contest::file() {
	StringView id = url_args.extractNext();
	// Early id validation
	if (id.size() != FILE_ID_LEN)
		return error404();

	try {
		DB::Statement stmt = db_conn.prepare(
			"SELECT name, round_id FROM files WHERE id=?");
		stmt.setString(1, id.to_string());

		DB::Result res = stmt.executeQuery();
		if (!res.next())
			return error404();

		string file_name = res[1];

		rpath.reset(getRoundPath(res[2]));
		if (!rpath)
			return; // getRoundPath has already set error

		// Edit file
		StringView next_arg = url_args.extractNext();
		if (next_arg == "edit")
			return editFile(id, file_name);

		// Delete file
		if (next_arg == "delete")
			return deleteFile(id, file_name);

		// Download file
		resp.headers["Content-Disposition"] =
			concat("attachment; filename=", file_name);

		resp.content = concat("files/", id);
		resp.content_type = server::HttpResponse::FILE;
		return;

	} catch (const std::exception& e) {
		ERRLOG_CAUGHT(e);
		return error500();
	}
}

void Contest::files(bool admin_view) {
	if (rpath->type != RoundType::CONTEST)
		return error404();

	// Add file
	if (url_args.isNext("add")) {
		url_args.extractNext();
		return addFile();
	}

	auto ender = contestTemplate("Files");
	if (admin_view)
		append("<a class=\"btn\" href=\"/c/", rpath->round_id, "/files/add\">"
			"Add file</a>\n");

	try {
		DB::Statement stmt = db_conn.prepare(
			"SELECT id, modified, name, description FROM files "
			"WHERE round_id=? ORDER BY modified DESC");
		stmt.setString(1, rpath->round_id);
		DB::Result res = stmt.executeQuery();

		if (res.rowCount() == 0) {
			append("<p>There are no files here yet</p>");
			return;
		}

		append("<table class=\"files\">\n"
			"<thead>"
				"<tr>"
					"<th class=\"time\">Modified</th>"
					"<th class=\"name\">File name</th>"
					"<th class=\"description\">Description</th>"
					"<th class=\"actions\">Actions</th>"
				"</tr>"
			"</thead>\n"
			"<tbody>\n");

		while (res.next()) {
			string id = res[1];
			append("<tr>"
				"<td>", res[2], "</td>"
				"<td><a href=\"/file/", id, "\">", htmlSpecialChars(res[3]),
					"</a></td>"
				"<td>", htmlSpecialChars(res[4]), "</td>"
				"<td><a class=\"btn-small\" href=\"/file/", id, "\">Download"
					"</a>");

			if (admin_view)
				append("<a class=\"btn-small blue\" href=\"/file/", id,
						"/edit\">Edit</a>"
					"<a class=\"btn-small red\" href=\"/file/", id, "/delete\">"
						"Delete</a>");

			append("</td>"
				"</tr>\n");
		}

		append("</tbody>\n"
			"</table>\n");

	} catch (const std::exception& e) {
		ERRLOG_CAUGHT(e);
		return error500();
	}
}

#include "contest.h"
#include "form_validator.h"

#include <sim/constants.h>
#include <simlib/debug.h>
#include <simlib/http/response.h>
#include <simlib/process.h>
#include <simlib/time.h>

using std::string;

void Contest::addFile() {
	// TODO: rewrite with modals + ajax
	if (!rpath->admin_access)
		return error403();

	FormValidator fv(req->form_data);
	string file_name, description;
	if (req->method == server::HttpRequest::POST) {
		if (fv.get("csrf_token") != Session::csrf_token)
			return error403();

		string user_file_name, file_path;

		// Validate all fields
		fv.validate(file_name, "file-name", "File name", FILE_NAME_MAX_LEN);

		fv.validateNotBlank(user_file_name, "file", "File");

		fv.validateFilePathNotEmpty(file_path, "file", "File");

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
					"(id, round_id, name, description, file_size, modified) "
					"VALUES(?, NULL, ?, ?, 0, ?)");
				stmt.setString(2, file_name);
				stmt.setString(3, description);
				stmt.setString(4, current_time);
				do {
					id = generateId(FILE_ID_LEN);
					stmt.setString(1, id);
				} while (stmt.executeUpdate() == 0);

				// Get file size
				struct stat64 sb;
				if (stat64(file_path.c_str(), &sb))
					THROW("stat('", file_path, "')", error(errno));

				uint64_t file_size = sb.st_size;

				// Move file
				string location = concat("files/", id);
				if (move(file_path, location))
					THROW("move('", file_path, "', '", location, "')",
						error(errno));

				stmt = db_conn.prepare(
					"UPDATE files SET round_id=?, file_size=? WHERE id=?");
				stmt.setString(1, rpath->round_id);
				stmt.setUInt64(2, file_size);
				stmt.setString(3, id);

				if (stmt.executeUpdate() != 1) {
					(void)unlink(concat("files/", id));
					THROW("Failed to update inserted file");
				}

				return redirect(concat("/c/", rpath->round_id, "/files"));

			} catch (const std::exception& e) {
				ERRLOG_CATCH(e);
				fv.addError("Internal server error");
			}
	}

	contestTemplate("Add file");
	printRoundPath();
	append(fv.errors(), "<div class=\"form-container\">"
			"<h1>Add file</h1>"
			"<form method=\"post\" enctype=\"multipart/form-data\">"
				// File name
				"<div class=\"field-group\">"
					"<label>File name</label>"
					"<input type=\"text\" name=\"file-name\" value=\"",
						htmlEscape(file_name), "\" size=\"24\" "
						"maxlength=\"", toStr(FILE_NAME_MAX_LEN), "\" "
						"placeholder=\"The same as name of uploaded file\">"
				"</div>"
				// File
				"<div class=\"field-group\">"
					"<label>File</label>"
					"<input type=\"file\" name=\"file\" required>"
				"</div>"
				// Description
				"<div class=\"field-group\">"
					"<label>Description</label>"
					"<textarea name=\"description\" maxlength=\"",
						toStr(FILE_DESCRIPTION_MAX_LEN), "\">",
						htmlEscape(description), "</textarea>"
				"</div>"

				"<div class=\"button-row\">"
					"<input class=\"btn blue\" type=\"submit\" value=\"Submit\">"
					"<a class=\"btn\" href=\"/c/", rpath->round_id ,"/files\">"
						"Go back</a>"
				"</div>"
			"</form>"
		"</div>");
}

void Contest::editFile(const StringView& id, string name) {
	if (!rpath->admin_access)
		return error403();

	FormValidator fv(req->form_data);
	string modified, description;
	if (req->method == server::HttpRequest::POST) {
		if (fv.get("csrf_token") != Session::csrf_token)
			return error403();

		// Validate all fields
		fv.validate(name, "file-name", "File name", FILE_NAME_MAX_LEN);

		fv.validate(description, "description", "Description",
			FILE_DESCRIPTION_MAX_LEN);

		string file_path = fv.getFilePath("file");
		bool is_file_reuploaded = (fv.get("file").size() && file_path.size());
		if (name.empty()) {
			name = fv.get("file");
			if (!is_file_reuploaded)
				fv.addError(
					"File name cannot be blank unless you upload a new file");
		}

		// If all fields are OK
		if (fv.noErrors())
			try {
				DB::Statement stmt;
				// File is being reuploaded
				if (is_file_reuploaded) {
					// Get file size
					struct stat64 sb;
					if (stat64(file_path.c_str(), &sb))
						THROW("stat('", file_path, "')", error(errno));

					uint64_t file_size = sb.st_size;

					// Move file
					string location = concat("files/", id);
					if (BLOCK_SIGNALS(move(file_path, location)))
						THROW("move('", file_path, "', '", location, "')",
							error(errno));

					stmt = db_conn.prepare("UPDATE files SET name=?, "
						"description=?, modified=?, file_size=? WHERE id=?");
					stmt.setUInt64(4, file_size);
					stmt.setString(5, id.to_string());

				} else {
					stmt = db_conn.prepare("UPDATE files "
						"SET name=?, description=?, modified=? WHERE id=?");
					stmt.setString(4, id.to_string());
				}

				string current_time = date("%Y-%m-%d %H:%M:%S");
				stmt.setString(1, name);
				stmt.setString(2, description);
				stmt.setString(3, current_time);
				stmt.executeUpdate();

				fv.addError("Update successful");

			} catch (const std::exception& e) {
				ERRLOG_CATCH(e);
				fv.addError("Internal server error");
			}
	}

	uint64_t file_size;
	try {
		DB::Statement stmt = db_conn.prepare(
			"SELECT name, description, file_size, modified FROM files "
			"WHERE id=?");
		stmt.setString(1, id.to_string());

		DB::Result res = stmt.executeQuery();
		if (!res.next())
			return error404();

		name = res[1];
		description = res[2];
		file_size = res.getInt64(3);
		modified = res[4];

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return error500();
	}

	string referer = req->headers.get("Referer");
	if (referer.empty())
		referer = concat("/c/", rpath->round_id, "/files");

	contestTemplate("Edit file");
	printRoundPath();
	append(fv.errors(), "<div class=\"form-container\">"
			"<h1>Edit file</h1>"
			"<form method=\"post\" enctype=\"multipart/form-data\">"
				// File name
				"<div class=\"field-group\">"
					"<label>File name</label>"
					"<input type=\"text\" name=\"file-name\" value=\"",
						htmlEscape(name), "\" size=\"24\" "
						"maxlength=\"", toStr(FILE_NAME_MAX_LEN), "\" "
						"placeholder=\"The same as name of reuploaded file\">"
				"</div>"
				// Reupload file
				"<div class=\"field-group\">"
					"<label>Reupload file</label>"
					"<input type=\"file\" name=\"file\">"
				"</div>"
				// Description
				"<div class=\"field-group\">"
					"<label>Description</label>"
					"<textarea name=\"description\" maxlength=\"",
						toStr(FILE_DESCRIPTION_MAX_LEN), "\">",
						htmlEscape(description), "</textarea>"
				"</div>"
				// File size
				"<div class=\"field-group\">"
					"<label>File size</label>"
					"<input type=\"text\" value=\"",
						humanizeFileSize(file_size), " (", toStr(file_size),
						" bytes)\" disabled>"
				"</div>"
				// Modified
				"<div class=\"field-group\">"
					"<label>Modified</label>"
					"<span datetime=\"", modified, "\" disabled>", modified,
						"<sup>UTC+0</sup></span>"
				"</div>"

				"<div class=\"button-row\">"
					"<input class=\"btn blue\" type=\"submit\" value=\"Update\">"
					"<a class=\"btn\" href=\"/c/", rpath->round_id ,"/files\">"
						"Go back</a>"
					"<a class=\"btn red\" href=\"/file/", id, "/delete?",
						referer, "\">Delete file</a>"
				"</div>"
			"</form>"
		"</div>");
}

void Contest::deleteFile(const StringView& id, const StringView& name) {
	if (!rpath->admin_access)
		return error403();

	FormValidator fv(req->form_data);
	if (req->method == server::HttpRequest::POST && fv.exist("delete"))
		try {
			if (fv.get("csrf_token") != Session::csrf_token)
				return error403();

			SignalBlocker signal_guard;
			DB::Statement stmt = db_conn.prepare(
				"DELETE FROM files WHERE id=?");
			stmt.setString(1, id.to_string());

			if (stmt.executeUpdate() != 1)
				return error500();

			if (unlink(concat("files/", id)))
				THROW("unlink('files/", id, "')", error(errno));

			signal_guard.unblock();

			string location = url_args.extractQuery().to_string();
			return redirect(location.empty() ? "/" : location);

		} catch (const std::exception& e) {
			ERRLOG_CATCH(e);
			fv.addError("Internal server error");
		}

	contestTemplate("Delete file");
	printRoundPath();

	// Referer or file page
	string referer = req->headers.get("Referer");
	string prev_referer = url_args.extractQuery().to_string();
	if (prev_referer.empty()) {
		if (referer.size())
			prev_referer = referer;
		else {
			referer = concat("/file/", id);
			prev_referer = concat("/c/", rpath->round_id, "/files");
		}

	} else if (referer.empty())
		referer = concat("/file/", id);

	append(fv.errors(), "<div class=\"form-container\">"
		"<h1>Delete file</h1>"
		"<form method=\"post\" action=\"?", prev_referer, "\">"
			"<label class=\"field\">Are you sure you want to delete file "
				"<a href=\"/file/", id, "/edit\">", htmlEscape(name), "</a>?"
			"</label>"
			"<div class=\"submit-yes-no\">"
				"<button class=\"btn red\" type=\"submit\" name=\"delete\">"
					"Yes, I'm sure</button>"
				"<a class=\"btn\" href=\"", referer, "\">No, go back</a>"
			"</div>"
		"</form>"
	"</div>");
}

void Contest::file() {
	StringView id = url_args.extractNextArg();
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
		StringView next_arg = url_args.extractNextArg();
		if (next_arg == "edit")
			return editFile(id, file_name);

		// Delete file
		if (next_arg == "delete")
			return deleteFile(id, file_name);

		// Download file
		resp.headers["Content-Disposition"] =
			concat("attachment; filename=", http::quote(file_name));

		resp.content = concat("files/", id);
		resp.content_type = server::HttpResponse::FILE;
		return;

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return error500();
	}
}

void Contest::files(bool admin_view) {
	if (rpath->type != RoundType::CONTEST)
		return error404();

	StringView next_arg = url_args.extractNextArg();
	// Add file
	if (next_arg == "add")
		return addFile();

	contestTemplate("Files");
	append("<h1>Files</h1>");
	if (admin_view)
		append("<a class=\"btn\" href=\"/c/", rpath->round_id, "/files/add\">"
			"Add file</a>");

	try {
		DB::Statement stmt = db_conn.prepare(
			"SELECT id, modified, name, file_size, description FROM files "
			"WHERE round_id=? ORDER BY modified DESC");
		stmt.setString(1, rpath->round_id);
		DB::Result res = stmt.executeQuery();

		if (res.rowCount() == 0) {
			append("<p>There are no files here yet</p>");
			return;
		}

		append("<table class=\"files\">"
			"<thead>"
				"<tr>"
					"<th class=\"time\">Modified</th>"
					"<th class=\"name\">File name</th>"
					"<th class=\"size\">File size</th>"
					"<th class=\"description\">Description</th>"
					"<th class=\"actions\">Actions</th>"
				"</tr>"
			"</thead>"
			"<tbody>");

		while (res.next()) {
			string id = res[1];
			append("<tr>"
				"<td datetime=\"", res[2],"\">", res[2], "<sup>UTC+0</sup></td>"
				"<td><a href=\"/file/", id, "\">", htmlEscape(res[3]),
					"</a></td>"
				"<td>", humanizeFileSize(res.getUInt64(4)), "</td>"
				"<td>", htmlEscape(res[5]), "</td>"
				"<td><a class=\"btn-small\" href=\"/file/", id, "\">Download"
					"</a>");

			if (admin_view)
				append("<a class=\"btn-small blue\" href=\"/file/", id,
						"/edit\">Edit</a>"
					"<a class=\"btn-small red\" href=\"/file/", id, "/delete\">"
						"Delete</a>");

			append("</td>"
				"</tr>");
		}

		append("</tbody>"
			"</table>");

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return error500();
	}
}

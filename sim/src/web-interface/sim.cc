#include "sim.h"

#include <simlib/filesystem.h>
// #include <sim/jobs.h>
// #include <simlib/debug.h>
// #include <simlib/logger.h>
// #include <simlib/time.h>

using std::string;
using std::unique_ptr;
using std::vector;

server::HttpResponse Sim::handle(CStringView _client_ip,
	server::HttpRequest req)
{
	client_ip = std::move(_client_ip);
	request = std::move(req);
	resp = server::HttpResponse(server::HttpResponse::TEXT);

	stdlog(request.target);

	// TODO: this is pretty bad-looking
	auto hard_error500 = [&] {
		resp.status_code = "500 Internal Server Error";
		resp.headers["Content-Type"] = "text/html; charset=utf-8";
		resp.content = "<!DOCTYPE html>"
				"<html lang=\"en\">"
				"<head><title>500 Internal Server Error</title></head>"
				"<body>"
					"<center>"
						"<h1>500 Internal Server Error</h1>"
						"<p>Try to reload the page in a few seconds.</p>"
						"<button onclick=\"history.go(0)\">Reload</button>"
						"</center>"
				"</body>"
			"</html>";
	};

	try {
		STACK_UNWINDING_MARK;
		try {
			STACK_UNWINDING_MARK;

			url_args = RequestURIParser {request.target};
			StringView next_arg = url_args.extractNextArg();

			// Reset state
			page_template_began = false;
			notifications.clear();
			session_is_open = false;
			form_validation_error = false;

			// Check CSRF token
			if (request.method == server::HttpRequest::POST) {
				// If no session is open, load value from cookie to pass
				// verification
				if (not session_open())
					session_csrf_token = request.getCookie("csrf_token");

				if (request.form_data.get("csrf_token") != session_csrf_token) {
					error403();
					goto cleanup;
				}
			}

			if (next_arg == "kit")
				static_file();

			// else if (next_arg == "c")
				// contest_handle();

			else if (next_arg == "s")
				submission_handle();

			else if (next_arg == "u")
				users_handle();

			else if (next_arg == "")
				main_page();

			else if (next_arg == "api")
				api_handle();

			// else if (next_arg == "p")
				// problemset_handle();

			else if (next_arg == "login")
				login();

			else if (next_arg == "jobs")
				jobs_handle();

			// else if (next_arg == "file")
				// contest_file();

			else if (next_arg == "logout")
				logout();

			else if (next_arg == "signup")
				sign_up();

			else if (next_arg == "logs")
				view_logs();

			else
				error404();

		cleanup:
			page_template_end();

			// Make sure that the session is closed
			session_close();

		} catch (const std::exception& e) {
			ERRLOG_CATCH(e);
			error500();

		} catch (...) {
			ERRLOG_CATCH();
			error500();
		}

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		// We cannot use error500() because it will probably throw
		hard_error500();

	} catch (...) {
		ERRLOG_CATCH();
		// We cannot use error500() because it will probably throw
		hard_error500();
	}

	return std::move(resp);
}

void Sim::main_page() {
	STACK_UNWINDING_MARK;

	page_template("Main page");
	append("<div style=\"text-align: center\">"
			"<img src=\"/kit/img/SIM-logo.png\" width=\"260\" height=\"336\" "
				"alt=\"\">"
			"<p style=\"font-size: 30px\">Welcome to SIM</p>"
			"<hr>"
			"<p>SIM is an open source platform for carrying out algorithmic "
				"contests</p>"
		"</div>");
}

void Sim::static_file() {
	STACK_UNWINDING_MARK;

	string file_path = concat_tostr("static",
			abspath(decodeURI(StringView{request.target}
				.substring(1, request.target.find('?')))));
	// Extract path (ignore query)
	D(stdlog(file_path);)

	// Get file stat
	struct stat attr;
	if (stat(file_path.c_str(), &attr) != -1) {
		// Extract time of last modification
		auto it = request.headers.find("if-modified-since");
		resp.headers["last-modified"] = date("%a, %d %b %Y %H:%M:%S GMT",
			attr.st_mtime);
		resp.setCache(true, 100 * 24 * 60 * 60); // 100 days

		// If "If-Modified-Since" header is set and its value is not lower than
		// attr.st_mtime
		struct tm client_mtime;
		if (it && strptime(it->second.c_str(),
				"%a, %d %b %Y %H:%M:%S GMT", &client_mtime) != nullptr &&
			timegm(&client_mtime) >= attr.st_mtime)
		{
			resp.status_code = "304 Not Modified";
			return;
		}
	}

	resp.content_type = server::HttpResponse::FILE;
	resp.content = file_path;
}

void Sim::view_logs() {
	STACK_UNWINDING_MARK;

	if (!session_open() || session_user_type > UserType::ADMIN)
		return error403();

	page_template("Logs", "body{padding-left:20px}");

	append("<h2>Server's log:</h2>"
		"<pre id=\"web\" class=\"logs\"></pre>"

		"<h2>Server's error log:</h2>"
		"<pre id=\"web_err\" class=\"logs\"></pre>"

		"<h2>Job server's log:</h2>"
		"<pre id=\"jobs\" class=\"logs\"></pre>"

		"<h2>Job server's error log:</h2>"
		"<pre id=\"jobs_err\" class=\"logs\"></pre>"

		"<script>"
			"new Logs('web', $('#web')).monitor_scroll();"
			"new Logs('web_err', $('#web_err')).monitor_scroll();"
			"new Logs('jobs', $('#jobs')).monitor_scroll();"
			"new Logs('jobs_err', $('#jobs_err')).monitor_scroll();"
		"</script>");
}
#if 0
void Sim::view_submission() {
	if (session_open())
		return redirect("/login?" + request.target);

	// Extract round id
	string submission_id;
	if (strToNum(submission_id, url_args.extractNextArg()) <= 0)
		return error404();

	try {
		StringView next_arg = url_args.extractNextArg();

		// Check if user forces the observer view
		bool admin_view = true;
		if (next_arg == "n") {
			admin_view = false;
			next_arg = url_args.extractNextArg();
		}

		enum class Query : uint8_t {
			DELETE, REJUDGE, CHANGE_TYPE, DOWNLOAD, RAW, VIEW_SOURCE, NONE
		} query = Query::NONE;

		if (next_arg == "delete")
			query = Query::DELETE;
		else if (next_arg == "rejudge")
			query = Query::REJUDGE;
		else if (next_arg == "change-type")
			query = Query::CHANGE_TYPE;
		else if (next_arg == "raw")
			query = Query::RAW;
		else if (next_arg == "download")
			query = Query::DOWNLOAD;
		else if (next_arg == "source")
			query = Query::VIEW_SOURCE;

		// Get the submission
		const char* columns = (query != Query::NONE ? "" : ", submit_time, "
			"status, score, name, label, first_name, last_name, username, "
			"initial_report, final_report");
		MySQL::Statement stmt = db_conn.prepare(concat(
			"SELECT s.owner, round_id, s.type, p.id, p.type", columns,
			" FROM submissions s"
			" LEFT JOIN users u ON s.owner=u.id"
			" STRAIGHT_JOIN problems p ON problem_id=p.id"
			" WHERE s.id=? AND s.type!=" STYPE_VOID_STR));
		stmt.setString(1, submission_id);

		MySQL::Result res = stmt.executeQuery();
		if (!res.next())
			return error404();

		string submission_owner = (res.isNull(1) ? "" : res[1]);
		string round_id = (res.isNull(2) ? "" : res[2]);
		SubmissionType stype = SubmissionType(res.getUInt(3));
		string problem_id = res[4];
		ProblemType problem_type = ProblemType(res.getUInt(5));

		auto pageTemplate = [&](auto&&... args) {
			if (round_id.empty()) {
				Problemset::problem_id_ = problem_id;
				Problemset::perms = Problemset::getPermissions(submission_owner,
					problem_type);
				problemsetTemplate(std::forward<decltype(args)>(args)...);
			} else {
				contestTemplate(std::forward<decltype(args)>(args)...);
				printRoundPath();
			}
		};

		// Check permissions
		bool has_admin_access;
		if (round_id.empty())
			has_admin_access = (Session::user_type == UTYPE_ADMIN);
		else {
			// Get parent rounds
			rpath.reset(getRoundPath(round_id));
			if (!rpath)
				return; // getRoundPath has already set error
			has_admin_access = rpath->admin_access;
		}

		if (!has_admin_access && Session::user_id != submission_owner)
			return error403();

		admin_view &= has_admin_access;

		/* Delete */
		if (query == Query::DELETE) {
			if (!admin_view || stype == SubmissionType::PROBLEM_SOLUTION)
				return error403();

			return round_id.empty() ?
				Problemset::deleteSubmission(submission_id, submission_owner)
				: Contest::deleteSubmission(submission_id, submission_owner);
		}

		/* Rejudge */
		if (query == Query::REJUDGE) {
			FormValidator fv(request.form_data);
			if (!admin_view || request.method != server::HttpRequest::POST ||
				fv.get("csrf_token") != Session::csrf_token)
			{
				return error403();
			}

			// Add a job to judge the submission
			stmt = db_conn.prepare("INSERT jobs (creator, status, priority,"
					" type, added, aux_id, info, data)"
				"VALUES(?, " JQSTATUS_PENDING_STR ", ?, ?, ?, ?, ?, '')");
			stmt.setString(1, Session::user_id);
			stmt.setUInt(2, priority(JobQueueType::JUDGE_SUBMISSION));
			stmt.setUInt(3, (uint)JobQueueType::JUDGE_SUBMISSION);
			stmt.setString(4, date());
			stmt.setString(5, submission_id);
			stmt.setString(6, jobs::dumpString(problem_id));
			stmt.executeUpdate();

			notifyJobServer();

			db_conn.executeUpdate("UPDATE submissions"
				" SET status=" SSTATUS_PENDING_STR
				" WHERE id=" + submission_id);

			return response("200 OK");
		}

		/* Change submission type */
		if (query == Query::CHANGE_TYPE) {
			// Changes are only allowed if one has permissions and only in the
			// pool: {NORMAL | FINAL, IGNORED}
			if (!admin_view || stype > SubmissionType::IGNORED)
				return error403();

			// Get new stype
			FormValidator fv(request.form_data);
			if (fv.get("csrf_token") != Session::csrf_token)
				return response("403 Forbidden");

			SubmissionType new_stype;
			{
				std::string new_stype_str = fv.get("stype");
				if (new_stype_str == "n/f")
					new_stype = SubmissionType::NORMAL;
				else if (new_stype_str == "i")
					new_stype = SubmissionType::IGNORED;
				else
					return response("501 Not Implemented");
			}

			// No change
			if ((stype == SubmissionType::IGNORED) ==
				(new_stype == SubmissionType::IGNORED))
			{
				return response("200 OK");
			}

			if (round_id.empty())
				Problemset::changeSubmissionTypeTo(submission_id,
					submission_owner, new_stype);
			else
				Contest::changeSubmissionTypeTo(submission_id, submission_owner,
					new_stype);

			return response("200 OK");
		}

		/* Raw code */
		if (query == Query::RAW) {
			resp.headers["Content-type"] = "text/plain; charset=utf-8";
			resp.headers["Content-Disposition"] =
				concat("inline; filename=", submission_id, ".cpp");

			resp.content = concat("solutions/", submission_id, ".cpp");
			resp.content_type = server::HttpResponse::FILE;
			return;
		}

		/* Download solution */
		if (query == Query::DOWNLOAD) {
			resp.headers["Content-type"] = "application/text";
			resp.headers["Content-Disposition"] =
				concat("attachment; filename=", submission_id, ".cpp");

			resp.content = concat("solutions/", submission_id, ".cpp");
			resp.content_type = server::HttpResponse::FILE;
			return;
		}

		/* View source */
		if (query == Query::VIEW_SOURCE) {
			pageTemplate(concat("Submission ", submission_id, " - source"));

			append("<h2>Source code of submission ", submission_id, "</h2>"
				"<div>"
					"<a class=\"btn-small\" href=\"/s/", submission_id, "\">"
						"View submission</a>"
					"<a class=\"btn-small\" href=\"/s/", submission_id,
						"/raw/", submission_id, ".cpp\">Raw code</a>"
					"<a class=\"btn-small\" href=\"/s/", submission_id,
						"/download\">Download</a>"
				"</div>",
				cpp_syntax_highlighter(getFileContents(
				concat("solutions/", submission_id, ".cpp"))));
			return;
		}

		string submit_time = res[6];
		SubmissionStatus submission_status = SubmissionStatus(res.getUInt(7));
		string score = res[8];
		string problems_name = res[9];
		string problems_label = res[10];
		string full_name = concat(res[11], ' ', res[12]);

		pageTemplate("Submission " + submission_id);

		append("<div class=\"submission-info\">"
			"<div>"
				"<h1>Submission ", submission_id, "</h1>"
				"<div>"
					"<a class=\"btn-small\" href=\"/s/", submission_id,
						"/source\">View source</a>"
					"<a class=\"btn-small\" href=\"/s/", submission_id,
						"/raw/", submission_id, ".cpp\">Raw code</a>"
					"<a class=\"btn-small\" href=\"/s/", submission_id,
						"/download\">Download</a>");
		if (admin_view) {
			if (stype != SubmissionType::PROBLEM_SOLUTION)
				append("<a class=\"btn-small orange\" "
					"onclick=\"changeSubmissionType(", submission_id, ",'",
						(stype <= SubmissionType::FINAL ? "n/f" : "i"), "')\">"
					"Change type</a>");

			append("<a class=\"btn-small blue\" onclick=\"rejudgeSubmission(",
					submission_id, ")\">Rejudge</a>");

			if (stype != SubmissionType::PROBLEM_SOLUTION)
				append("<a class=\"btn-small red\" href=\"/s/", submission_id,
					"/delete?/", (round_id.empty() ?
						StringBuff<100>{"p/", problem_id}
						: StringBuff<100>{"c/", round_id}),
					"/submissions\">Delete</a>");
		}

		append("</div>"
			"</div>"
			"<table>"
				"<thead>"
					"<tr>");

		if (admin_view && stype != SubmissionType::PROBLEM_SOLUTION)
			append("<th style=\"min-width:120px\">User</th>");

		append("<th style=\"min-width:120px\">Problem</th>"
						"<th style=\"min-width:150px\">Submission time</th>"
						"<th style=\"min-width:150px\">Status</th>"
						"<th style=\"min-width:90px\">Score</th>"
						"<th style=\"min-width:90px\">Type</th>"
					"</tr>"
				"</thead>"
				"<tbody>");

		if (stype == SubmissionType::IGNORED)
			append("<tr class=\"ignored\">");
		else
			append("<tr>");

		if (admin_view && stype != SubmissionType::PROBLEM_SOLUTION)
			append("<td><a href=\"/u/", submission_owner, "\">",
					res[13], "</a> (", htmlEscape(full_name), ")</td>");

		bool show_final_results = (admin_view || round_id.empty() ||
			rpath->round->full_results <= date());

		append("<td><a href=\"/p/", problem_id, "\">",
					htmlEscape(problems_name), "</a>"
					" (", htmlEscape(problems_label), ')', "</td>"
				"<td datetime=\"", submit_time ,"\">", submit_time,
					"<sup>UTC</sup></td>",
				submissionStatusAsTd(submission_status, show_final_results),
				"<td>", (show_final_results ? score : ""), "</td>"
				"<td>", stype, "</td>"
			"</tr>"
			"</tbody>"
			"</table>",
			"</div>");

		// Print judge report
		append("<div class=\"results\">");
		// Initial report
		string initial_report = res[14];
		if (initial_report.size())
			append(initial_report);
		// Final report
		if (show_final_results) {
			string final_report = res[15];
			if (final_report.size())
				append(final_report);
		}

		append("</div>");

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return error500();
	}
}
#endif

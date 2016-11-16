#include "contest.h"
#include "form_validator.h"

#include <simlib/debug.h>
#include <simlib/process.h>
#include <simlib/time.h>

using std::string;
using std::vector;

void Contest::submit(bool admin_view) {
	// TODO: admin views as normal - before round begins, admin does not get 403
	// error
	if (!Session::open())
		return redirect("/login?" + req->target);

	FormValidator fv(req->form_data);
	bool ignored_submission = false;

	if (req->method == server::HttpRequest::POST) {
		if (fv.get("csrf_token") != Session::csrf_token)
			return error403();

		string solution, problem_round_id, solution_tmp_path;

		// Validate all fields
		fv.validateNotBlank(solution, "solution", "Solution file field");

		fv.validateNotBlank(problem_round_id, "round-id", "Problem");

		fv.validateFilePathNotEmpty(solution_tmp_path, "solution",
			"Solution file field");

		ignored_submission = (admin_view && fv.exist("ignored-submission"));

		if (!isDigit(problem_round_id))
			fv.addError("Wrong problem round id");

		// If all fields are ok
		// TODO: "transaction"
		if (fv.noErrors()) {
			std::unique_ptr<RoundPath> path;
			const RoundPath* problem_r_path = rpath.get();

			if (rpath->type != PROBLEM) {
				// Get parent rounds of problem round (it also checks access
				// permissions)
				path.reset(getRoundPath(problem_round_id));
				if (!path)
					return; // getRoundPath has already set error

				if (path->type != PROBLEM) {
					fv.addError("Wrong problem round id");
					goto form;
				}

				problem_r_path = path.get();
			}

			struct stat sb;
			if (stat(solution_tmp_path.c_str(), &sb))
				THROW("stat('", solution_tmp_path, "')", error(errno));

			// Check if solution is too big
			if ((uint64_t)sb.st_size > SOLUTION_MAX_SIZE) {
				fv.addError(concat("Solution file is too big (maximum allowed "
					"size: ", toStr(SOLUTION_MAX_SIZE), " bytes = ",
					toStr(SOLUTION_MAX_SIZE >> 10), " KB)"));
				goto form;
			}

			try {
				string current_date = date("%Y-%m-%d %H:%M:%S");
				// Insert submission to `submissions`
				DB::Statement stmt = db_conn.prepare("INSERT submissions "
						"(user_id, problem_id, round_id, parent_round_id, "
							"contest_round_id, submit_time, queued, status,"
							"initial_report, final_report) "
						"VALUES(?, ?, ?, ?, ?, ?, ?, 0, '', '')");
				stmt.setString(1, Session::user_id);
				stmt.setString(2, problem_r_path->problem->problem_id);
				stmt.setString(3, problem_r_path->problem->id);
				stmt.setString(4, problem_r_path->round->id);
				stmt.setString(5, problem_r_path->contest->id);
				stmt.setString(6, current_date);
				stmt.setString(7, current_date);

				if (stmt.executeUpdate() != 1) {
					fv.addError("Database error - failed to insert submission");
					goto form;
				}

				// Get inserted submission id
				DB::Result res = db_conn.executeQuery(
					"SELECT LAST_INSERT_ID()");

				if (!res.next())
					THROW("Failed to get inserted submission id");

				string submission_id = res[1];

				// Copy solution
				string location = concat("solutions/", submission_id, ".cpp");
				if (copy(solution_tmp_path, location))
					THROW("copy('", solution_tmp_path, "', '", location, "')",
						error(errno));

				// Change submission type to NORMAL (activate submission)
				stmt = db_conn.prepare("UPDATE submissions SET type=?, status="
					SSTATUS_WAITING_STR " WHERE id=?");
				stmt.setUInt(1, static_cast<uint>(ignored_submission
					? SubmissionType::IGNORED : SubmissionType::NORMAL));
				stmt.setString(2, submission_id);
				stmt.executeUpdate();

				notifyJudgeServer();

				return redirect("/s/" + submission_id);

			} catch (const std::exception& e) {
				ERRLOG_CATCH(e);
				fv.addError("Internal server error");
			}
		}
	}

 form:
	contestTemplate("Submit a solution");
	printRoundPath();
	string buffer;
	back_insert(buffer, fv.errors(), "<div class=\"form-container\">"
			"<h1>Submit a solution</h1>"
			"<form method=\"post\" enctype=\"multipart/form-data\">"
				// Round id
				"<div class=\"field-group\">"
					"<label>Problem</label>"
					"<select name=\"round-id\">");

	// List problems
	try {
		string current_date = date("%Y-%m-%d %H:%M:%S");
		if (rpath->type == CONTEST) {
			// Select subrounds
			// Admin -> All problems from all subrounds
			// Normal -> All problems from subrounds which have begun and
			// have not ended
			DB::Statement stmt = db_conn.prepare(admin_view ?
					"SELECT id, name FROM rounds WHERE parent=? ORDER BY item"
				: "SELECT id, name FROM rounds WHERE parent=? "
					"AND (begins IS NULL OR begins<=?) "
					"AND (ends IS NULL OR ?<ends) ORDER BY item");
			stmt.setString(1, rpath->contest->id);
			if (!admin_view) {
				stmt.setString(2, current_date);
				stmt.setString(3, current_date);
			}

			DB::Result res = stmt.executeQuery();
			vector<Subround> subrounds;
			// For performance
			subrounds.reserve(res.rowCount());

			// Collect results
			while (res.next()) {
				subrounds.emplace_back();
				subrounds.back().id = res[1];
				subrounds.back().name = res[2];
			}

			// Select problems
			stmt = db_conn.prepare("SELECT id, parent, name FROM rounds "
				"WHERE grandparent=? ORDER BY item");
			stmt.setString(1, rpath->contest->id);
			res = stmt.executeQuery();

			// (round_id, problems)
			std::map<string, vector<Problem> > problems_table;

			// Fill problems with all subrounds
			for (auto&& sr : subrounds)
				problems_table[sr.id];

			// Collect results
			while (res.next()) {
				// Get reference to proper vector<Problem>
				auto it = problems_table.find(res[2]);
				// If problem parent is not visible or database error
				if (it == problems_table.end())
					continue; // Ignore

				vector<Problem>& prob = it->second;
				prob.emplace_back();
				prob.back().id = res[1];
				prob.back().parent = res[2];
				prob.back().name = res[3];
			}

			// For each subround list all problems
			for (auto&& sr : subrounds) {
				vector<Problem>& prob = problems_table[sr.id];

				for (auto& problem : prob)
					back_insert(buffer, "<option value=\"", problem.id, "\">",
						htmlSpecialChars(problem.name), " (",
						htmlSpecialChars(sr.name), ")</option>");
			}

		// Admin -> All problems
		// Normal -> if round has begun and has not ended
		} else if (rpath->type == ROUND && (admin_view || (
			rpath->round->begins <= current_date && // "" <= everything
			(rpath->round->ends.empty() || current_date < rpath->round->ends))))
		{
			// Select problems
			DB::Statement stmt = db_conn.prepare(
				"SELECT id, name FROM rounds WHERE parent=? ORDER BY item");
			stmt.setString(1, rpath->round->id);

			// List problems
			DB::Result res = stmt.executeQuery();
			while (res.next())
				back_insert(buffer, "<option value=\"", res[1],
					"\">", htmlSpecialChars(res[2]),
					" (", htmlSpecialChars(rpath->round->name),
					")</option>");

		// Admin -> Current problem
		// Normal -> if parent round has begun and has not ended
		} else if (rpath->type == PROBLEM && (admin_view || (
			rpath->round->begins <= current_date && // "" <= everything
			(rpath->round->ends.empty() || current_date < rpath->round->ends))))
		{
			back_insert(buffer, "<option value=\"", rpath->problem->id,
				"\">", htmlSpecialChars(rpath->problem->name), " (",
				htmlSpecialChars(rpath->round->name), ")</option>");
		}

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		fv.addError("Internal server error");
	}

	if (hasSuffix(buffer, "</option>")) {
		append(buffer, "</select>"
					"</div>"
					// Solution file
					"<div class=\"field-group\">"
						"<label>Solution</label>"
						"<input type=\"file\" name=\"solution\" required>"
					"</div>");

		if (admin_view) // Ignored submission
			append("<div class=\"field-group\">"
						"<label>Ignored submission</label>"
						"<input type=\"checkbox\" name=\"ignored-submission\"",
							(ignored_submission ? " checked" : ""), ">"
				"</div>");

		append("<input class=\"btn blue\" type=\"submit\" value=\"Submit\">"
				"</form>"
			"</div>");
	}

	else
		append("<p>There are no problems for which you can submit a solution..."
			"</p>");
}

void Contest::deleteSubmission(const string& submission_id,
	const string& submission_user_id)
{
	FormValidator fv(req->form_data);
	while (req->method == server::HttpRequest::POST
		&& fv.exist("delete"))
	{
		if (fv.get("csrf_token") != Session::csrf_token)
			return error403();

		try {
			SignalBlocker signal_guard;
			// Update FINAL status, as there was no submission submission_id
			DB::Statement stmt = db_conn.prepare(
				"UPDATE submissions SET type=" STYPE_FINAL_STR " "
				"WHERE user_id=? AND round_id=? AND id!=? "
					"AND type<=" STYPE_FINAL_STR " "
					"AND status<" SSTATUS_WAITING_STR " "
				"ORDER BY id DESC LIMIT 1");
			stmt.setString(1, submission_user_id);
			stmt.setString(2, rpath->round_id);
			stmt.setString(3, submission_id);
			stmt.executeUpdate();

			// Delete submission
			stmt = db_conn.prepare(
				"DELETE FROM submissions WHERE id=?");
			stmt.setString(1, submission_id);

			if (stmt.executeUpdate() == 0) {
				fv.addError("Deletion failed");
				break;
			}

			signal_guard.unblock();

			string location = url_args.extractQuery().to_string();
			return redirect(location.empty() ? "/" : location);

		} catch (const std::exception& e) {
			fv.addError("Internal server error");
			ERRLOG_CATCH(e);
			break;
		}
	}

	contestTemplate("Delete submission");
	printRoundPath();

	// Referer or submission page
	string referer = req->headers.get("Referer");
	string prev_referer = url_args.extractQuery().to_string();
	if (prev_referer.empty()) {
		if (referer.size())
			prev_referer = referer;
		else {
			referer = concat("/s/", submission_id);
			prev_referer = concat("/c/", rpath->round_id, "/submissions");
		}

	} else if (referer.empty())
		referer = concat("/s/", submission_id);

	append(fv.errors(), "<div class=\"form-container\">"
			"<h1>Delete submission</h1>"
			"<form method=\"post\" action=\"?", prev_referer ,"\">"
				"<label class=\"field\">Are you sure you want to delete "
					"submission <a href=\"/s/", submission_id, "\">",
					submission_id, "</a>?</label>"
				"<div class=\"submit-yes-no\">"
					"<button class=\"btn red\" type=\"submit\" "
						"name=\"delete\">Yes, I'm sure</button>"
					"<a class=\"btn\" href=\"", referer, "\">No, go back</a>"
				"</div>"
			"</form>"
		"</div>");
}

void Contest::submission() {
	if (!Session::open())
		return redirect("/login?" + req->target);

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

		// Get submission
		const char* columns = (query != Query::NONE ? "" : ", submit_time, "
			"status, score, name, abbreviation, first_name, last_name, "
			"username, initial_report, final_report");
		DB::Statement stmt = db_conn.prepare(
			concat("SELECT user_id, round_id, s.type", columns, " "
				"FROM submissions s, problems p, users u "
				"WHERE s.id=? AND s.problem_id=p.id AND u.id=user_id"));
		stmt.setString(1, submission_id);

		DB::Result res = stmt.executeQuery();
		if (!res.next())
			return error404();

		string submission_user_id = res[1];
		string round_id = res[2];
		SubmissionType stype = SubmissionType(res.getUInt(3));

		// Get parent rounds
		rpath.reset(getRoundPath(round_id));
		if (!rpath)
			return; // getRoundPath has already set error

		if (!rpath->admin_access && Session::user_id != submission_user_id)
			return error403();

		if (admin_view)
			admin_view = rpath->admin_access;

		/* Delete */
		if (query == Query::DELETE)
			return (admin_view ? deleteSubmission(submission_id,
				submission_user_id) : error403());

		/* Rejudge */
		if (query == Query::REJUDGE) {
			if (!admin_view)
				return error403();

			// TODO: make it as 'Change type', and use CSRF protection!

			stmt = db_conn.prepare("UPDATE submissions "
				"SET status=" SSTATUS_WAITING_STR ", queued=? WHERE id=?");
			stmt.setString(1, date("%Y-%m-%d %H:%M:%S"));
			stmt.setString(2, submission_id);

			stmt.executeUpdate();
			notifyJudgeServer();

			// Redirect to Referer or submission page
			string referer = req->headers.get("Referer");
			if (referer.empty())
				return redirect(concat("/s/", submission_id));

			return redirect(referer);
		}

		/* Change submission type */
		if (query == Query::CHANGE_TYPE) {
			// Changes are only allowed if one has permissions and only in the
			// pool: {NORMAL | FINAL, IGNORED}
			if (!admin_view || stype > SubmissionType::IGNORED)
				return response("403 Forbidden");

			// Get new stype
			FormValidator fv(req->form_data);
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

			try {
				// Set to IGNORED
				if (new_stype == SubmissionType::IGNORED) {
					stmt = db_conn.prepare("UPDATE submissions s, "
						"((SELECT id FROM submissions WHERE user_id=? "
								"AND round_id=? AND id!=? "
								"AND type<=" STYPE_FINAL_STR " "
								"AND status<" SSTATUS_WAITING_STR " "
								"ORDER BY id DESC LIMIT 1) "
							"UNION (SELECT 0 AS id)) x,"
						"((SELECT id FROM submissions WHERE user_id=? "
								"AND round_id=? AND type=" STYPE_FINAL_STR ") "
							"UNION (SELECT 0 AS id)) y "
						"SET s.type=IF(s.id=x.id," STYPE_FINAL_STR ",IF(s.id=?,"
							STYPE_IGNORED_STR "," STYPE_NORMAL_STR "))"
						"WHERE s.id=x.id OR s.id=y.id OR s.id=?");

					stmt.setString(7, submission_id);

				// Set to NORMAL / FINAL
				} else {
					stmt = db_conn.prepare("UPDATE submissions s, "
						"((SELECT id FROM submissions WHERE user_id=? "
								"AND round_id=? "
								"AND (id=? OR type<=" STYPE_FINAL_STR ") "
								"AND status<" SSTATUS_WAITING_STR " "
								"ORDER BY id DESC LIMIT 1) "
							"UNION (SELECT 0 AS id)) x,"
						"((SELECT id FROM submissions WHERE user_id=? "
								"AND round_id=? AND type=" STYPE_FINAL_STR ") "
							"UNION (SELECT 0 AS id)) y "
						"SET s.type=IF(s.id=x.id," STYPE_FINAL_STR ","
							STYPE_NORMAL_STR ")"
						"WHERE s.id=x.id OR s.id=y.id OR s.id=?");
				}
				stmt.setString(1, submission_user_id);
				stmt.setString(2, round_id);
				stmt.setString(3, submission_id);
				stmt.setString(4, submission_user_id);
				stmt.setString(5, round_id);
				stmt.setString(6, submission_id);
				stmt.executeUpdate();

				return response("200 OK");

			} catch (const std::exception& e) {
				ERRLOG_CATCH(e);
				return response("500 Internal Server Error");
			}
		}

		/* Raw code */
		if (query == Query::RAW) {
			resp.headers["Content-type"] = "text/plain";

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
			contestTemplate(concat("Submission ", submission_id, " - source"));
			printRoundPath();

			append("<h2>Source code of submission ", submission_id, "</h2>"
				"<div>"
					"<a class=\"btn-small\" href=\"/s/", submission_id, "\">"
						"View submission</a>"
					"<a class=\"btn-small\" href=\"/s/", submission_id,
						"/raw/", submission_id, ".cpp\">Raw code</a>"
				"</div>",
				cpp_syntax_highlighter(getFileContents(
				concat("solutions/", submission_id, ".cpp"))));
			return;
		}

		string submit_time = res[4];
		SubmissionStatus submission_status = SubmissionStatus(res.getUInt(5));
		string score = res[6];
		string problem_name = res[7];
		string problem_abbreviation = res[8];
		string full_name = concat(res[9], ' ', res[10]);

		contestTemplate("Submission " + submission_id);
		printRoundPath();

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
		if (admin_view)
			append("<a class=\"btn-small orange\" "
				"onclick=\"changeSubmissionType(", submission_id, ",'",
					(stype <= SubmissionType::FINAL ? "n/f" : "i"), "')\">"
					"Change type</a>"
				"<a class=\"btn-small blue\" href=\"/s/", submission_id,
					"/rejudge\">Rejudge</a>"
				"<a class=\"btn-small red\" href=\"/s/", submission_id,
					"/delete?/c/", round_id, "/submissions\">Delete</a>");
		append("</div>"
			"</div>"
			"<table style=\"width: 100%\">"
				"<thead>"
					"<tr>");

		if (admin_view)
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

		if (admin_view)
			append("<td><a href=\"/u/", submission_user_id, "\">",
					res[11], "</a> (", htmlSpecialChars(full_name), ")</td>");

		bool show_final_results = (admin_view ||
			rpath->round->full_results <= date("%Y-%m-%d %H:%M:%S"));

		append("<td>", htmlSpecialChars(
				concat(problem_name, " (", problem_abbreviation, ')')), "</td>"
				"<td datetime=\"", toStr(strToTime(submit_time)),"\">",
					submit_time, "<sup>UTC+0</sup></td>",
				submissionStatusAsTd(submission_status, show_final_results),
				"<td>", (show_final_results ? score : ""), "</td>"
				"<td>", toStr(stype), "</td>"
			"</tr>"
			"</tbody>"
			"</table>",
			"</div>");

		// Print judge report
		append("<div class=\"results\">");
		// Initial report
		string initial_report = res[12];
		if (initial_report.size())
			append(initial_report);
		// Final report
		if (admin_view ||
			rpath->round->full_results <= date("%Y-%m-%d %H:%M:%S"))
		{
			string final_report = res[13];
			if (final_report.size())
				append(final_report);
		}

		append("</div>");

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return error500();
	}
}

void Contest::submissions(bool admin_view) {
	if (!Session::isOpen())
		return redirect("/login?" + req->target);

	contestTemplate("Submissions");
	append("<h1>Submissions</h1>");
	printRoundPath("submissions", !admin_view);

	append("<h3>Submission queue size: ");
	try {
		DB::Result res = db_conn.executeQuery(
			"SELECT COUNT(id) FROM submissions WHERE status="
			SSTATUS_WAITING_STR);
		if (res.next())
			append(res[1]);

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return error500();
	}

	append("</h3>");

	// Select submissions
	try {
		const char* param_column = (rpath->type == CONTEST ? "contest_round_id"
			: (rpath->type == ROUND ? "parent_round_id" : "round_id"));

		DB::Statement stmt = db_conn.prepare(admin_view ?
				concat("SELECT s.id, s.submit_time, r2.id, r2.name, r.id, "
					"r.name, s.status, s.score, s.type, s.user_id, "
					"u.first_name, u.last_name, u.username "
				"FROM submissions s, rounds r, rounds r2, users u "
				"WHERE s.", param_column, "=? AND s.round_id=r.id "
					"AND s.parent_round_id=r2.id AND s.user_id=u.id "
				"ORDER BY s.id DESC")
			: concat("SELECT s.id, s.submit_time, r2.id, r2.name, r.id, "
					"r.name, s.status, s.score, s.type, r2.full_results "
				"FROM submissions s, rounds r, rounds r2 "
				"WHERE s.", param_column, "=? AND s.round_id=r.id "
					"AND s.parent_round_id=r2.id AND s.user_id=? "
				"ORDER BY s.id DESC"));
		stmt.setString(1, rpath->round_id);
		if (!admin_view)
			stmt.setString(2, Session::user_id);

		DB::Result res = stmt.executeQuery();
		if (res.rowCount() == 0) {
			append("<p>There are no submissions to show</p>");
			return;
		}

		append("<table class=\"submissions\">"
			"<thead>"
				"<tr>",
					(admin_view ? "<th class=\"username\">Username</th>"
							"<th class=\"full-name\">Full name</th>"
						: ""),
					"<th class=\"time\">Submission time<sup>UTC+0</sup></th>"
					"<th class=\"problem\">Problem</th>"
					"<th class=\"status\">Status</th>"
					"<th class=\"score\">Score</th>"
					"<th class=\"type\">Type</th>"
					"<th class=\"actions\">Actions</th>"
				"</tr>"
			"</thead>"
			"<tbody>");

		string current_date = date("%Y-%m-%d %H:%M:%S");
		while (res.next()) {
			SubmissionType stype = SubmissionType(res.getUInt(9));
			if (stype == SubmissionType::IGNORED)
				append("<tr class=\"ignored\">");
			else
				append("<tr>");

			// User
			if (admin_view)
				append("<td><a href=\"/u/", res[10], "\">", res[13],"</a></td>"
					"<td>", htmlSpecialChars(concat(res[11], ' ', res[12])),
					"</td>");

			bool show_final_results = (admin_view || res[10] <= current_date);

			// Rest
			append("<td><a href=\"/s/", res[1], "\" datetime=\"",
						toStr(strToTime(res[2])),"\">", res[2], "</a></td>"
					"<td>"
						"<a href=\"/c/", res[3], "\">",
							htmlSpecialChars(res[4]), "</a>"
						" ~> "
						"<a href=\"/c/", res[5], "\">",
							htmlSpecialChars(res[6]), "</a>"
					"</td>",
					submissionStatusAsTd(SubmissionStatus(res.getUInt(7)),
						show_final_results),
					"<td>", (show_final_results ? res[8] : ""), "</td>"
					"<td>", toStr(stype), "</td>"
					"<td>"
						"<a class=\"btn-small\" href=\"/s/", res[1],
							"/source\">View source</a>"
						"<a class=\"btn-small\" href=\"/s/", res[1],
							"/download\">Download</a>");

			if (admin_view)
				append("<a class=\"btn-small orange\" "
						"onclick=\"changeSubmissionType(", res[1], ",'",
						(stype <= SubmissionType::FINAL ? "n/f" : "i"), "')\">"
						"Change type</a>"
					"<a class=\"btn-small blue\" href=\"/s/", res[1],
						"/rejudge\">Rejudge</a>"
					"<a class=\"btn-small red\" href=\"/s/", res[1],
						"/delete\">Delete</a>");

			append("</td>"
				"<tr>");
		}

		append("</tbody>"
			"</table>");

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return error500();
	}
}

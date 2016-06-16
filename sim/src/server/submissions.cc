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

	if (req->method == server::HttpRequest::POST) {
		string solution, problem_round_id;

		// Validate all fields
		fv.validateNotBlank(solution, "solution", "Solution file field");

		fv.validateNotBlank(problem_round_id, "round-id", "Problem");

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

			string solution_tmp_path = fv.getFilePath("solution");
			struct stat sb;
			if (stat(solution_tmp_path.c_str(), &sb))
				THROW("stat()", error(errno));

			// Check if solution is too big
			if ((uint64_t)sb.st_size > SOLUTION_MAX_SIZE) {
				fv.addError(concat("Solution file to big (max ",
					toStr(SOLUTION_MAX_SIZE), " bytes = ",
					toString(SOLUTION_MAX_SIZE >> 10), " KiB)"));
				goto form;
			}

			try {
				string current_date = date("%Y-%m-%d %H:%M:%S");
				// Insert submission to `submissions`
				DB::Statement stmt = db_conn.prepare("INSERT submissions "
						"(user_id, problem_id, round_id, parent_round_id, "
							"contest_round_id, submit_time, queued,"
							"initial_report, final_report) "
						"VALUES(?, ?, ?, ?, ?, ?, ?, '', '')");
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
				if (copy(solution_tmp_path,
					concat("solutions/", submission_id, ".cpp")))
				{
					THROW("copy()", error(errno));
				}

				// Change submission status to 'waiting'
				db_conn.executeUpdate("UPDATE submissions SET status='waiting' "
					"WHERE id=" + submission_id);

				notifyJudgeServer();

				return redirect("/s/" + submission_id);

			} catch (const std::exception& e) {
				fv.addError("Internal server error");
				ERRLOG_CATCH(e);
			}
		}
	}

 form:
	contestTemplate("Submit a solution");
	printRoundPath();
	string buffer;
	back_insert(buffer, fv.errors(), "<div class=\"form-container\">\n"
			"<h1>Submit a solution</h1>\n"
			"<form method=\"post\" enctype=\"multipart/form-data\">\n"
				// Round id
				"<div class=\"field-group\">\n"
					"<label>Problem</label>\n"
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
						htmlSpecialChars(sr.name), ")</option>\n");
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
					")</option>\n");

		// Admin -> Current problem
		// Normal -> if parent round has begun and has not ended
		} else if (rpath->type == PROBLEM && (admin_view || (
			rpath->round->begins <= current_date && // "" <= everything
			(rpath->round->ends.empty() || current_date < rpath->round->ends))))
		{
			back_insert(buffer, "<option value=\"", rpath->problem->id,
				"\">", htmlSpecialChars(rpath->problem->name), " (",
				htmlSpecialChars(rpath->round->name), ")</option>\n");
		}

	} catch (const std::exception& e) {
		fv.addError("Internal server error");
		ERRLOG_CATCH(e);
	}

	if (hasSuffix(buffer, "</option>\n"))
		append(buffer, "</select>"
					"</div>\n"
					// Solution file
					"<div class=\"field-group\">\n"
						"<label>Solution</label>\n"
						"<input type=\"file\" name=\"solution\" required>\n"
					"</div>\n"
					"<input class=\"btn blue\" type=\"submit\" value=\"Submit\">\n"
				"</form>\n"
			"</div>\n");

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
		try {
			SignalBlocker signal_guard;
			// Update `final` status as there was no submission submission_id
			DB::Statement stmt = db_conn.prepare(
				"UPDATE submissions SET final=1 "
				"WHERE user_id=? AND round_id=? AND id!=? "
					"AND (status='ok' OR status='error') "
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

	append(fv.errors(), "<div class=\"form-container\">\n"
			"<h1>Delete submission</h1>\n"
			"<form method=\"post\" action=\"?", prev_referer ,"\">\n"
				"<label class=\"field\">Are you sure to delete submission "
				"<a href=\"/s/", submission_id, "\">", submission_id,
					"</a>?</label>\n"
				"<div class=\"submit-yes-no\">\n"
					"<button class=\"btn red\" type=\"submit\" "
						"name=\"delete\">Yes, I'm sure</button>\n"
					"<a class=\"btn\" href=\"", referer, "\">No, go back</a>\n"
				"</div>\n"
			"</form>\n"
		"</div>\n");
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
			DELETE, REJUDGE, DOWNLOAD, VIEW_SOURCE, NONE
		} query = Query::NONE;

		if (next_arg == "delete")
			query = Query::DELETE;
		else if (next_arg == "rejudge")
			query = Query::REJUDGE;
		else if (next_arg == "download")
			query = Query::DOWNLOAD;
		else if (next_arg == "source")
			query = Query::VIEW_SOURCE;

		// Get submission
		const char* columns = (query != Query::NONE ? "" : ", submit_time, "
			"status, score, name, tag, first_name, last_name, username, "
			"initial_report, final_report");
		DB::Statement stmt = db_conn.prepare(
			concat("SELECT user_id, round_id", columns, " "
				"FROM submissions s, problems p, users u "
				"WHERE s.id=? AND s.problem_id=p.id AND u.id=user_id"));
		stmt.setString(1, submission_id);

		DB::Result res = stmt.executeQuery();
		if (!res.next())
			return error404();

		string submission_user_id = res[1];
		string round_id = res[2];

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

			stmt = db_conn.prepare("UPDATE submissions "
				"SET status='waiting', queued=? WHERE id=?");
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

		/* Download solution */
		if (query == Query::DOWNLOAD) {
			resp.headers["Content-type"] = "application/text";
			resp.headers["Content-Disposition"] =
				concat("attachment; filename=", submission_id, ".cpp");

			resp.content = concat("solutions/", submission_id, ".cpp");
			resp.content_type = server::HttpResponse::FILE;
			return;
		}

		contestTemplate("Submission " + submission_id);
		printRoundPath();

		/* View source */
		if (query == Query::VIEW_SOURCE) {
			append(cpp_syntax_highlighter(getFileContents(
				concat("solutions/", submission_id, ".cpp"))));
			return;
		}

		string submit_time = res[3];
		string submission_status = res[4];
		string score = res[5];
		string problem_name = res[6];
		string problem_tag = res[7];
		string full_name = concat(res[8], ' ', res[9]);

		append("<div class=\"submission-info\">\n"
			"<div>\n"
				"<h1>Submission ", submission_id, "</h1>\n"
				"<div>\n"
					"<a class=\"btn-small\" href=\"/s/", submission_id,
						"/source\">View source</a>\n"
					"<a class=\"btn-small\" href=\"/s/", submission_id,
						"/download\">Download</a>\n");
		if (admin_view)
			append("<a class=\"btn-small blue\" href=\"/s/", submission_id,
					"/rejudge\">Rejudge</a>\n"
				"<a class=\"btn-small red\" href=\"/s/", submission_id,
					"/delete?/c/", round_id, "/submissions\">Delete</a>\n");
		append("</div>\n"
			"</div>\n"
			"<table style=\"width: 100%\">\n"
				"<thead>\n"
					"<tr>");

		if (admin_view)
			append("<th style=\"min-width:120px\">User</th>");

		append("<th style=\"min-width:120px\">Problem</th>"
						"<th style=\"min-width:150px\">Submission time</th>"
						"<th style=\"min-width:150px\">Status</th>"
						"<th style=\"min-width:90px\">Score</th>"
					"</tr>\n"
				"</thead>\n"
				"<tbody>\n"
					"<tr>");

		if (admin_view)
			append("<td><a href=\"/u/", submission_user_id, "\">",
					res[10], "</a> (", htmlSpecialChars(full_name), ")</td>");

		append("<td>", htmlSpecialChars(
			concat(problem_name, " (", problem_tag, ')')), "</td>"
				"<td>", htmlSpecialChars(submit_time), "</td>"
				"<td");

		if (submission_status == "ok")
			append(" class=\"ok\"");
		else if (submission_status == "error")
			append(" class=\"wa\"");
		else if (submission_status == "c_error")
			append(" class=\"tl-rte\"");
		else if (submission_status == "judge_error")
			append(" class=\"judge-error\"");

		append('>', submissionStatusDescription(submission_status),
						"</td>"
						"<td>", (admin_view ||
							rpath->round->full_results.empty() ||
							rpath->round->full_results <=
								date("%Y-%m-%d %H:%M:%S") ? score : ""),
						"</td>"
					"</tr>\n"
				"</tbody>\n"
			"</table>\n",
			"</div>\n");

		// Print judge report
		append("<div class=\"results\">");
		if (admin_view || rpath->round->full_results.empty() ||
			rpath->round->full_results <= date("%Y-%m-%d %H:%M:%S"))
		{
			string final_report = res[12];
			if (final_report.size())
				append(final_report);
		}

		string initial_report = res[11];
		if (initial_report.size()) // TODO: fix bug - empty initial report table
			append(initial_report);

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
			"SELECT COUNT(id) FROM submissions WHERE status='waiting';");
		if (res.next())
			append(res[1]);

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return error500();
	}

	append( "</h3>");

	try {
		const char* param_column = (rpath->type == CONTEST ? "contest_round_id"
			: (rpath->type == ROUND ? "parent_round_id" : "round_id"));

		DB::Statement stmt = db_conn.prepare(admin_view ?
				concat("SELECT s.id, s.submit_time, r2.id, r2.name, r.id, "
					"r.name, s.status, s.score, s.final, s.user_id, "
					"u.first_name, u.last_name, u.username "
				"FROM submissions s, rounds r, rounds r2, users u "
				"WHERE s.", param_column, "=? AND s.round_id=r.id "
					"AND s.parent_round_id=r2.id AND s.user_id=u.id "
				"ORDER BY s.id DESC")
			: concat("SELECT s.id, s.submit_time, r2.id, r2.name, r.id, "
					"r.name, s.status, s.score, s.final, r2.full_results "
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

		append("<table class=\"submissions\">\n"
			"<thead>\n"
				"<tr>",
					(admin_view ? "<th class=\"username\">Username</th>"
							"<th class=\"full-name\">Full name</th>"
						: ""),
					"<th class=\"time\">Submission time</th>"
					"<th class=\"problem\">Problem</th>"
					"<th class=\"status\">Status</th>"
					"<th class=\"score\">Score</th>"
					"<th class=\"final\">Final</th>"
					"<th class=\"actions\">Actions</th>"
				"</tr>\n"
			"</thead>\n"
			"<tbody>\n");

		auto statusRow = [](const string& status) {
			string ret = "<td";

			if (status == "ok")
				ret += " class=\"ok\">";
			else if (status == "error")
				ret += " class=\"wa\">";
			else if (status == "c_error")
				ret += " class=\"tl-rte\">";
			else if (status == "judge_error")
				ret += " class=\"judge-error\">";
			else
				ret += ">";

			return back_insert(ret, submissionStatusDescription(status),
				"</td>");
		};

		string current_date = date("%Y-%m-%d %H:%M:%S");
		while (res.next()) {
			append("<tr>");
			// User
			if (admin_view)
				append("<td><a href=\"/u/", res[10], "\">", res[13],"</a></td>"
					"<td>", htmlSpecialChars(concat(res[11], ' ', res[12])),
					"</td>");
			// Rest
			append("<td><a href=\"/s/", res[1], "\">", res[2], "</a></td>"
					"<td>"
						"<a href=\"/c/", res[3], "\">",
							htmlSpecialChars(res[4]), "</a>"
						" ~> "
						"<a href=\"/c/", res[5], "\">",
							htmlSpecialChars(res[6]), "</a>"
					"</td>",
					statusRow(res[7]),
					"<td>", (admin_view || string(res[10]) <= current_date
						? res[8] : ""), "</td>"
					"<td>", (res.getBool(9) ? "Yes" : ""), "</td>"
					"<td>"
						"<a class=\"btn-small\" href=\"/s/", res[1],
							"/source\">View source</a>"
						"<a class=\"btn-small\" href=\"/s/", res[1],
							"/download\">Download</a>");

			if (admin_view)
				append("<a class=\"btn-small blue\" href=\"/s/", res[1],
						"/rejudge\">Rejudge</a>"
					"<a class=\"btn-small red\" href=\"/s/", res[1],
						"/delete\">Delete</a>");

			append("</td>"
				"<tr>\n");
		}

		append("</tbody>\n"
			"</table>\n");

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return error500();
	}
}

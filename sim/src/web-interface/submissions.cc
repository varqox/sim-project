#include "contest.h"
#include "form_validator.h"

#include <sim/jobs.h>
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
	string code;

	if (req->method == server::HttpRequest::POST) {
		if (fv.get("csrf_token") != Session::csrf_token)
			return error403();

		string solution, problem_round_id, solution_tmp_path;

		// Validate all fields
		fv.validateNotBlank(problem_round_id, "round-id", "Problem");

		code = fv.get("code");

		solution_tmp_path = fv.getFilePath("solution");

		ignored_submission = (admin_view && fv.exist("ignored-submission"));

		if (!isDigit(problem_round_id))
			fv.addError("Wrong problem round id");

		// Only one option: code or file may be chosen
		if ((code.empty() ^ fv.get("solution").empty()) == 0)
			fv.addError("You have to either choose a file or paste the code");

		// If all fields are ok
		// TODO: "transaction"
		if (fv.noErrors()) {
			std::unique_ptr<RoundPath> path;
			const RoundPath* problem_rpath = rpath.get();

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

				problem_rpath = path.get();
			}

			// Check solution size
			if (code.empty()) { // File
				struct stat sb;
				if (stat(solution_tmp_path.c_str(), &sb))
					THROW("stat('", solution_tmp_path, "')", error(errno));

				// Check if solution is too big
				if ((uint64_t)sb.st_size > SOLUTION_MAX_SIZE) {
					fv.addError(concat("Solution is too big (maximum allowed "
						"size: ", SOLUTION_MAX_SIZE, " bytes = ",
						SOLUTION_MAX_SIZE >> 10, " KB)"));
					goto form;
				}

			} else if (code.size() > SOLUTION_MAX_SIZE) { // Code
				fv.addError(concat("Solution is too big (maximum allowed "
					"size: ", SOLUTION_MAX_SIZE, " bytes = ",
					SOLUTION_MAX_SIZE >> 10, " KB)"));
				goto form;
			}

			try {
				string current_date = date();
				// Insert submission to the `submissions` table
				MySQL::Statement stmt = db_conn.prepare(
					"INSERT submissions (owner, problem_id, round_id,"
						" parent_round_id, contest_round_id, type, status,"
						" submit_time, last_judgment, initial_report,"
						" final_report)"
					" VALUES(?, ?, ?, ?, ?, " STYPE_VOID_STR ", "
						SSTATUS_PENDING_STR ", ?, ?, '', '')");
				stmt.setString(1, Session::user_id);
				stmt.setString(2, problem_rpath->problem->problem_id);
				stmt.setString(3, problem_rpath->problem->id);
				stmt.setString(4, problem_rpath->round->id);
				stmt.setString(5, problem_rpath->contest->id);
				stmt.setString(6, current_date);
				stmt.setString(7, date("%Y-%m-%d %H:%M:%S", 0));

				if (stmt.executeUpdate() != 1) {
					fv.addError("Database error - failed to insert submission");
					goto form;
				}

				// Get inserted submission id
				string submission_id = db_conn.lastInsertId();

				// Copy solution
				string location = concat("solutions/", submission_id, ".cpp");
				if (code.empty()) { // File
					if (copy(solution_tmp_path, location))
						THROW("copy(`", solution_tmp_path, "`, `", location,
							"`)", error(errno));

				} else if (putFileContents(location, code) == -1) // Code
					THROW("putFileContents(`", location, "`, ...)",
						error(errno));

				// Add a job to judge the submission
				stmt = db_conn.prepare("INSERT job_queue (creator, type,"
						" priority, status, added, aux_id, info, data)"
					" VALUES(?, " JQTYPE_VOID_STR ", ?, " JQSTATUS_PENDING_STR
						", ?, ?, ?, '')");
				stmt.setString(1, Session::user_id);
				stmt.setUInt(2, priority(JobQueueType::JUDGE_SUBMISSION));
				stmt.setString(3, date());
				stmt.setString(4, submission_id);
				stmt.setString(5,
					jobs::dumpString(problem_rpath->problem->problem_id));
				stmt.executeUpdate();

				// Activate the submission and the job (change type to a valid
				// one)
				stmt = db_conn.prepare("UPDATE submissions s, job_queue j"
					" SET s.type=?, j.type=" JQTYPE_JUDGE_SUBMISSION_STR
					" WHERE s.id=? AND j.id=LAST_INSERT_ID()");
				stmt.setUInt(1, static_cast<uint>(ignored_submission
					? SubmissionType::IGNORED : SubmissionType::NORMAL));
				stmt.setString(2, submission_id);
				stmt.executeUpdate();

				notifyJobServer();

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
		string current_date = date();
		if (rpath->type == CONTEST) {
			// Select subrounds
			// Admin -> All problems from all subrounds
			// Normal -> All problems from subrounds which have begun and
			// have not ended
			MySQL::Statement stmt = db_conn.prepare(admin_view ?
					"SELECT id, name FROM rounds WHERE parent=? ORDER BY item"
				: "SELECT id, name FROM rounds WHERE parent=? "
					"AND (begins IS NULL OR begins<=?) "
					"AND (ends IS NULL OR ?<ends) ORDER BY item");
			stmt.setString(1, rpath->contest->id);
			if (!admin_view) {
				stmt.setString(2, current_date);
				stmt.setString(3, current_date);
			}

			MySQL::Result res = stmt.executeQuery();
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
						htmlEscape(problem.name), " (", htmlEscape(sr.name),
						")</option>");
			}

		// Admin -> All problems
		// Normal -> if round has begun and has not ended
		} else if (rpath->type == ROUND && (admin_view || (
			rpath->round->begins <= current_date && // "" <= everything
			(rpath->round->ends.empty() || current_date < rpath->round->ends))))
		{
			// Select problems
			MySQL::Statement stmt = db_conn.prepare(
				"SELECT id, name FROM rounds WHERE parent=? ORDER BY item");
			stmt.setString(1, rpath->round->id);

			// List problems
			MySQL::Result res = stmt.executeQuery();
			while (res.next())
				back_insert(buffer, "<option value=\"", res[1],
					"\">", htmlEscape(res[2]), " (",
					htmlEscape(rpath->round->name), ")</option>");

		// Admin -> Current problem
		// Normal -> if parent round has begun and has not ended
		} else if (rpath->type == PROBLEM && (admin_view || (
			rpath->round->begins <= current_date && // "" <= everything
			(rpath->round->ends.empty() || current_date < rpath->round->ends))))
		{
			back_insert(buffer, "<option value=\"", rpath->problem->id,
				"\">", htmlEscape(rpath->problem->name), " (",
				htmlEscape(rpath->round->name), ")</option>");
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
						"<input type=\"file\" name=\"solution\">"
					"</div>"
					"<div class=\"field-group\">"
						"<label>Code</label>"
						"<textarea class=\"monospace\" rows=\"8\" cols=\"50\" "
							"name=\"code\">", htmlEscape(code), "</textarea>"
					"</div>");

		// TODO: bound labels to fields (using "for" attribute)

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
	const string& submission_owner)
{
	FormValidator fv(req->form_data);
	while (req->method == server::HttpRequest::POST
		&& fv.exist("delete"))
	{
		if (fv.get("csrf_token") != Session::csrf_token)
			return error403();

		try {
			SignalBlocker signal_guard;
			// Update FINAL status, as if there was no submission submission_id
			MySQL::Statement stmt = db_conn.prepare(
				"UPDATE submissions SET type=" STYPE_FINAL_STR " "
				"WHERE owner=? AND round_id=? AND id!=? "
					"AND type<=" STYPE_FINAL_STR " "
					"AND status<" SSTATUS_PENDING_STR " "
				"ORDER BY id DESC LIMIT 1");
			stmt.setString(1, submission_owner);
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

void Contest::changeSubmissionTypeTo(const string& submission_id,
	const string& submission_owner, SubmissionType stype)
{
	try {
		static_assert(int(SubmissionType::NORMAL) == 0 &&
			int(SubmissionType::FINAL) == 1, "Needed below where "
				"\"... type<= ...\"");
		MySQL::Statement stmt;
		// Set to IGNORED
		if (stype == SubmissionType::IGNORED) {
			stmt = db_conn.prepare("UPDATE submissions s, "
				"((SELECT id FROM submissions WHERE owner=? "
						"AND round_id=? AND id!=? "
						"AND type<=" STYPE_FINAL_STR " "
						"AND status<" SSTATUS_PENDING_STR " "
						"ORDER BY id DESC LIMIT 1) "
					"UNION (SELECT 0 AS id)) x,"
				"((SELECT id FROM submissions WHERE owner=? "
						"AND round_id=? AND type=" STYPE_FINAL_STR ") "
					"UNION (SELECT 0 AS id)) y "
				"SET s.type=IF(s.id=x.id," STYPE_FINAL_STR ",IF(s.id=?,"
					STYPE_IGNORED_STR "," STYPE_NORMAL_STR "))"
				"WHERE s.id=x.id OR s.id=y.id OR s.id=?");

			stmt.setString(7, submission_id);

		// Set to NORMAL / FINAL
		} else {
			stmt = db_conn.prepare("UPDATE submissions s, "
				"((SELECT id FROM submissions WHERE owner=? "
						"AND round_id=? "
						"AND (id=? OR type<=" STYPE_FINAL_STR ") "
						"AND status<" SSTATUS_PENDING_STR " "
						"ORDER BY id DESC LIMIT 1) "
					"UNION (SELECT 0 AS id)) x,"
				"((SELECT id FROM submissions WHERE owner=? "
						"AND round_id=? AND type=" STYPE_FINAL_STR ") "
					"UNION (SELECT 0 AS id)) y "
				"SET s.type=IF(s.id=x.id," STYPE_FINAL_STR ","
					STYPE_NORMAL_STR ")"
				"WHERE s.id=x.id OR s.id=y.id OR s.id=?");
		}
		stmt.setString(1, submission_owner);
		stmt.setString(2, rpath->round_id);
		stmt.setString(3, submission_id);
		stmt.setString(4, submission_owner);
		stmt.setString(5, rpath->round_id);
		stmt.setString(6, submission_id);
		stmt.executeUpdate();

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return response("500 Internal Server Error");
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
		MySQL::Result res = db_conn.executeQuery(
			"SELECT COUNT(id) FROM submissions WHERE status="
			SSTATUS_PENDING_STR); // TODO: job-queue is now used
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

		MySQL::Statement stmt = db_conn.prepare(admin_view ?
				concat("SELECT s.id, s.submit_time, r2.id, r2.name, r.id,"
					" r.name, s.status, s.score, s.type, s.owner,"
					" u.first_name, u.last_name, u.username"
				" FROM submissions s, rounds r, rounds r2, users u"
				" WHERE s.", param_column, "=? AND s.type!=" STYPE_VOID_STR
					" AND s.round_id=r.id AND s.parent_round_id=r2.id"
					" AND s.owner=u.id"
				" ORDER BY s.id DESC")
			: concat("SELECT s.id, s.submit_time, r2.id, r2.name, r.id,"
					" r.name, s.status, s.score, s.type, r2.full_results"
				" FROM submissions s, rounds r, rounds r2"
				" WHERE s.", param_column, "=? AND s.type!=" STYPE_VOID_STR
					" AND s.round_id=r.id AND s.parent_round_id=r2.id"
					" AND s.owner=?"
				" ORDER BY s.id DESC"));
		stmt.setString(1, rpath->round_id);
		if (!admin_view)
			stmt.setString(2, Session::user_id);

		MySQL::Result res = stmt.executeQuery();
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
					"<th class=\"time\">Submission time<sup>UTC</sup></th>"
					"<th class=\"problem\">Problem</th>"
					"<th class=\"status\">Status</th>"
					"<th class=\"score\">Score</th>"
					"<th class=\"type\">Type</th>"
					"<th class=\"actions\">Actions</th>"
				"</tr>"
			"</thead>"
			"<tbody>");

		string current_date = date();
		while (res.next()) {
			SubmissionType stype = SubmissionType(res.getUInt(9));
			if (stype == SubmissionType::IGNORED)
				append("<tr class=\"ignored\">");
			else
				append("<tr>");

			// User
			if (admin_view)
				append("<td><a href=\"/u/", res[10], "\">", res[13],"</a></td>"
					"<td>", htmlEscape(concat(res[11], ' ', res[12])), "</td>");

			bool show_final_results = (admin_view || res[10] <= current_date);

			// Rest
			append("<td><a href=\"/s/", res[1], "\" datetime=\"", res[2], "\">",
						res[2], "</a></td>"
					"<td>"
						"<a href=\"/c/", res[3], "\">",
							htmlEscape(res[4]), "</a>"
						" ~> "
						"<a href=\"/c/", res[5], "\">",
							htmlEscape(res[6]), "</a>"
					"</td>",
					submissionStatusAsTd(SubmissionStatus(res.getUInt(7)),
						show_final_results),
					"<td>", (show_final_results ? res[8] : ""), "</td>"
					"<td>", stype, "</td>"
					"<td>"
						"<a class=\"btn-small\" href=\"/s/", res[1],
							"\">View</a>"
						"<a class=\"btn-small\" href=\"/s/", res[1],
							"/source\">Source</a>"
						"<a class=\"btn-small\" href=\"/s/", res[1],
							"/download\">Download</a>");

			if (admin_view)
				append("<a class=\"btn-small orange\" "
						"onclick=\"changeSubmissionType(", res[1], ",'",
						(stype <= SubmissionType::FINAL ? "n/f" : "i"), "')\">"
						"Change type</a>"
					"<a class=\"btn-small blue\" onclick=\"rejudgeSubmission(",
						res[1], ")\">Rejudge</a>"
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

#include "sim.h"

void Sim::submission_handle() {
	STACK_UNWINDING_MARK;

	if (not session_open())
		return redirect("/login?" + request.target);

	StringView submission_id = url_args.extractNextArg();
	if (not isDigit(submission_id))
		return error404();

	page_template(concat("Submission ", submission_id), "body{padding-left:32px}");
	append("<script>preview_submission(false, ", submission_id, ");</script>");
}

#if 0
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
					THROW("stat('", solution_tmp_path, "')", error());

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
							"`)", error());

				} else if (putFileContents(location, code) == -1) // Code
					THROW("putFileContents(`", location, "`, ...)", error());

				// Add a job to judge the submission
				stmt = db_conn.prepare("INSERT jobs (creator, type, priority,"
						" status, added, aux_id, info, data)"
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
				stmt = db_conn.prepare("UPDATE submissions s, jobs j"
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
			AVLDictMap<string, vector<Problem> > problems_table;

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
#endif

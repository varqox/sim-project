#include "form_validator.h"
#include "problemset.h"

#include <sim/jobs.h>
#include <simlib/config_file.h>
#include <simlib/http/response.h>
#include <simlib/sim/simfile.h>
#include <simlib/time.h>

using std::string;

Problemset::Permissions Problemset::getPermissions(const string& owner_id,
	ProblemType ptype)
{
	if (Session::open()) {
		if (Session::user_type == UTYPE_ADMIN)
			return Permissions(PERM_ADD | PERM_VIEW | PERM_VIEW_ALL |
				PERM_VIEW_SOLUTIONS | PERM_DOWNLOAD | PERM_SEE_SIMFILE | PERM_SEE_OWNER |
				PERM_ADMIN);

		if (Session::user_id == owner_id)
			return Permissions(PERM_VIEW | PERM_VIEW_SOLUTIONS | PERM_DOWNLOAD |
				PERM_SEE_SIMFILE | PERM_SEE_OWNER | PERM_ADMIN |
				(Session::user_type == UTYPE_TEACHER ? (int)PERM_ADD : 0));

		if (Session::user_type == UTYPE_TEACHER && ptype == ProblemType::PUBLIC)
			return Permissions(PERM_ADD | PERM_VIEW | PERM_VIEW_SOLUTIONS |
				PERM_DOWNLOAD | PERM_SEE_SIMFILE | PERM_SEE_OWNER);
	}

	return (ptype == ProblemType::PUBLIC ? PERM_VIEW : PERM_NONE);
}

void Problemset::problemsetTemplate(StringView title, StringView styles,
	StringView scripts)
{
	baseTemplate(title, concat("body{margin-left:190px}", styles),
		scripts);

	append("<ul class=\"menu\">"
			"<span>PROBLEMSET</span>");
	if (perms & PERM_ADD)
		append("<a href=\"/p/add\">Add a problem</a>"
			"<hr>");

	append("<a href=\"/p\">All problems</a>"
			"<a href=\"/p/my\">My problems</a>");
	if (!problem_id_.empty()) {
		append("<span>SELECTED PROBLEM</span>"
			"<a href=\"/p/", problem_id_, "\">View problem</a>"
			"<a href=\"/p/", problem_id_, "/statement\">View statement</a>");

		if (perms & PERM_VIEW_SOLUTIONS)
			append("<a href=\"/p/", problem_id_, "/solutions\">Solutions</a>");
		if (perms & PERM_ADMIN) {
			append("<a href=\"/p/", problem_id_, "/edit\">Edit problem</a>");
			append("<a href=\"/p/", problem_id_, "/submissions\">"
				"Submissions</a>");
		}

		append("<a href=\"/p/", problem_id_, "/submissions/my\">"
			"My submissions</a>");
	}
	append("</ul>");
}

void Problemset::handle() {
	StringView next_arg = url_args.extractNextArg();
	if (isDigit(next_arg)) {
		problem_id_ = next_arg.to_string();
		return problem();
	}

	problem_id_.clear();
	perms = getPermissions("", ProblemType::VOID); // Get permissions to overall
	                                               // problemset

	if (next_arg == "add")
		return addProblem();

	if (next_arg == "my") {
		if (!Session::isOpen())
			return redirect("/login?" + req->target);

	} else if (!next_arg.empty())
		return error404();

	problemsetTemplate("Problemset");
	append(next_arg.empty() ? "<h1>Problemset</h1>" : "<h1>My problems</h1>");
	if (perms & PERM_ADD)
		append("<a class=\"btn\" href=\"/p/add\">Add a problem</a>");

	// List available problems
	bool show_owner = (perms & PERM_SEE_OWNER);
	try {
		MySQL::Statement stmt;
		if (next_arg == "my") {
			show_owner = false;
			stmt = db_conn.prepare("SELECT id, name, label, added, type"
				" FROM problems WHERE owner=? AND type!=" PTYPE_VOID_STR
				" ORDER BY id");
			stmt.setString(1, Session::user_id);

		} else if (perms & PERM_VIEW_ALL) {
			stmt = db_conn.prepare("SELECT p.id, name, label, added, p.type,"
					" owner, username FROM problems p, users u"
				" WHERE owner=u.id AND p.type!=" PTYPE_VOID_STR " ORDER BY id");

		} else if (perms & PERM_SEE_OWNER) {
			stmt = db_conn.prepare("SELECT p.id, name, label, added, p.type,"
				" owner, username FROM problems p, users u"
				" WHERE owner=u.id AND (p.type=" PTYPE_PUBLIC_STR " OR owner=?)"
					" AND p.type!=" PTYPE_VOID_STR " ORDER BY id");
			stmt.setString(1, Session::user_id);

		} else if (Session::isOpen()) {
			stmt = db_conn.prepare("SELECT id, name, label, added, type,"
				" owner FROM problems"
				" WHERE type=" PTYPE_PUBLIC_STR
					" OR (owner=? AND type!=" PTYPE_VOID_STR ")"
				" ORDER BY id");
			stmt.setString(1, Session::user_id);

		} else {
			stmt = db_conn.prepare("SELECT id, name, label, added, type,"
				" owner FROM problems WHERE type=" PTYPE_PUBLIC_STR
				" ORDER BY id");
		}

		MySQL::Result res = stmt.executeQuery();
		if (!res.next()) {
			append("<p>There are no problems to show...</p>");
			return;
		}

		// TODO: add search option
		append("<table class=\"problems\">"
			"<thead>"
				"<tr>"
					"<th class=\"id\">Id</th>"
					"<th class=\"name\">Name</th>"
					"<th class=\"label\">Label</th>",
					(show_owner ? "<th class=\"owner\">Owner</th>" : ""),
					"<th class=\"added\">Added<sup>UTC</sup></th>"
					"<th class=\"is_public\">Is public</th>"
					"<th class=\"actions\">Actions</th>"
				"</tr>"
			"</thead>"
			"<tbody>");

		do {
			append("<tr>"
					"<td>", res[1], "</td>"
					"<td>", htmlEscape(res[2]), "</td>"
					"<td>", htmlEscape(res[3]), "</td>");

			if (show_owner) // Owner
				append("<td><a href=\"/u/", res[6], "\">", res[7], "</a></td>");

			Permissions problem_perms =
				getPermissions(next_arg == "my" ? Session::user_id : res[6],
					ProblemType(res.getUInt(5)));

			append("<td datetime=\"", res[4], "\">", res[4], "</td>"
					"<td>", (res.getUInt(5) == int(ProblemType::PUBLIC) ?
						"YES" : "NO"), "</td>"
					"<td>"
						"<a class=\"btn-small\" href=\"/p/", res[1], "\">"
							"View</a>"
						"<a class=\"btn-small\" href=\"/p/", res[1],
							"/statement\">Statement</a>");

			// More actions
			if (problem_perms & PERM_VIEW_SOLUTIONS)
				append("<a class=\"btn-small\" href=\"/p/", res[1],
					"/solutions\">Solutions</a>");

			if (problem_perms & PERM_ADMIN)
				append("<a class=\"btn-small blue\" href=\"/p/", res[1],
					"/edit\">Edit</a>");

			append("</td>"
				"</tr>");
		} while (res.next());

		append("</tbody>"
			"</table>");

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return error500();
	}
}

void Problemset::problemStatement(StringView problem_id) {
	// Get statement path
	ConfigFile cf;
	cf.addVars("statement");
	try {
		MySQL::Statement stmt =
			db_conn.prepare("SELECT simfile FROM problems WHERE id=?");
		stmt.setString(1, problem_id.to_string());

		MySQL::Result res = stmt.executeQuery();
		if (res.next())
			cf.loadConfigFromString(res[1]);
		else
			return error404();

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return error500();
	}

	auto&& statement = cf["statement"].asString();

	if (hasSuffix(statement, ".pdf"))
		resp.headers["Content-type"] = "application/pdf";
	else if (hasSuffixIn(statement, {".html", ".htm"}))
		resp.headers["Content-type"] = "text/html";
	else if (hasSuffixIn(statement, {".txt", ".md"}))
		resp.headers["Content-type"] = "text/plain; charset=utf-8";

	resp.headers["Content-Disposition"] =
		concat("inline; filename=", http::quote(filename(statement)));

	resp.content_type = server::HttpResponse::FILE;
	resp.content = concat("problems/", problem_id, '/', statement);
}

void Problemset::addProblem() {
	if (~perms & PERM_ADD)
		return error403();

	FormValidator fv(req->form_data);
	string name, label, memory_limit, user_package_file, global_time_limit;
	bool force_auto_limit = true, ignore_simfile = false,
		public_problem = false;

	if (req->method == server::HttpRequest::POST) {
		if (fv.get("csrf_token") != Session::csrf_token)
			return error403();

		string package_file;

		// Validate all fields
		fv.validate(name, "name", "Problem's name", PROBLEM_NAME_MAX_LEN);

		fv.validate(label, "label", "Problem's label", PROBLEM_LABEL_MAX_LEN);

		fv.validate(memory_limit, "memory-limit", "Memory limit",
			isDigitNotGreaterThan<(std::numeric_limits<uint64_t>::max() >> 20)>,
			"Memory limit: invalid value");

		fv.validate<bool(StringView)>(global_time_limit,
			"global-time-limit", "Global time limit", isReal,
			"Global time limit: invalid value"); // TODO: add length limit
		// Global time limit in usec
		uint64_t gtl =
			round(strtod(global_time_limit.c_str(), nullptr) * 1'000'000);
		if (global_time_limit.size() && gtl < 400000)
			fv.addError("Global time limit cannot be lower than 0.4 s");

		force_auto_limit = fv.exist("force-auto-limit");

		ignore_simfile = fv.exist("ignore-simfile");

		public_problem = fv.exist("public-problem");

		fv.validateNotBlank(user_package_file, "package", "Package");

		fv.validateFilePathNotEmpty(package_file, "package", "Package");

		// If all fields are OK
		if (fv.noErrors())
			try {
				jobs::AddProblemInfo ap_info {
					name,
					label,
					strtoull(memory_limit.c_str()),
					gtl,
					force_auto_limit,
					ignore_simfile,
					public_problem,
					-1
				};

				MySQL::Statement stmt {db_conn.prepare(
					"INSERT job_queue (creator, priority, type, status, added,"
						" info, data) "
					"VALUES(?, ?, " JQTYPE_VOID_STR ", " JQSTATUS_PENDING_STR
						", ?, ?, '')")};
				stmt.setString(1, Session::user_id);
				stmt.setUInt(2, priority(JobQueueType::ADD_PROBLEM));
				stmt.setString(3, date());
				stmt.setString(4, ap_info.dump());
				stmt.executeUpdate();

				string jobid = db_conn.lastInsertId();

				// Move package file that it will become a job's file
				{
					StringBuff<PATH_MAX> new_path
						{"jobs_files/", jobid, ".zip"};
					if (::move(package_file, new_path))
						THROW("Error: link(`", package_file, "`, `", new_path,
							"`)", error(errno));

					package_file = new_path.data();
				}

				// Activate the job
				int rc = db_conn.executeUpdate("UPDATE job_queue SET type="
					JQTYPE_ADD_PROBLEM_STR " WHERE id=" + jobid);
				if (1 != rc)
					THROW("Failed to update");

				notifyJobServer();
				return redirect(concat("/jobs/", jobid));

			} catch (const std::exception& e) {
				ERRLOG_CATCH(e);
				fv.addError("Internal server error");
			}
	}

	problemsetTemplate("Add a problem");
	append(fv.errors(), "<div class=\"form-container\">"
			"<h1>Add a problem</h1>"
			"<form method=\"post\" enctype=\"multipart/form-data\">"
				// Name
				"<div class=\"field-group\">"
					"<label>Problem's name</label>"
					"<input type=\"text\" name=\"name\" value=\"",
						htmlEscape(name), "\" size=\"24\""
					"maxlength=\"", PROBLEM_NAME_MAX_LEN, "\" "
					"placeholder=\"Detect from Simfile\">"
				"</div>"
				// Label
				"<div class=\"field-group\">"
					"<label>Problem's label</label>"
					"<input type=\"text\" name=\"label\" value=\"",
						htmlEscape(label), "\" size=\"24\""
					"maxlength=\"", PROBLEM_LABEL_MAX_LEN, "\" "
					"placeholder=\"Detect from Simfile or from name\">"
				"</div>"
				// Memory limit
				"<div class=\"field-group\">"
					"<label>Memory limit [MB]</label>"
					"<input type=\"text\" name=\"memory-limit\" value=\"",
						htmlEscape(memory_limit), "\" size=\"24\" "
					"placeholder=\"Detect from Simfile\">"
				"</div>"
				// Global time limit
				"<div class=\"field-group\">"
					"<label>Global time limit [s] (for each test)</label>"
					"<input type=\"text\" name=\"global-time-limit\" value=\"",
						htmlEscape(global_time_limit), "\" size=\"24\" "
					"placeholder=\"No global time limit\">"
				"</div>"
				// Force auto limit
				"<div class=\"field-group\">"
					"<label>Force automatic time limit setting</label>"
					"<input type=\"checkbox\" name=\"force-auto-limit\"",
						(force_auto_limit ? " checked" : ""), ">"
				"</div>"
				// Ignore Simfile
				"<div class=\"field-group\">"
					"<label>Ignore Simfile</label>"
					"<input type=\"checkbox\" name=\"ignore-simfile\"",
						(ignore_simfile ? " checked" : ""), ">"
				"</div>"
				// Package
				"<div class=\"field-group\">"
					"<label>Package [ZIP file]</label>"
					"<input type=\"file\" name=\"package\" required>"
				"</div>"
				// Public problem?
				"<div class=\"field-group\">"
					"<label>Public problem</label>"
					"<input type=\"checkbox\" name=\"public-problem\"",
						(public_problem ? " checked" : ""), ">"
				"</div>"
				"<input class=\"btn blue\" type=\"submit\" value=\"Submit\">"
			"</form>"
		"</div>");
}

void Problemset::problem() {
	// Fetch the problem info
	try {
		MySQL::Statement stmt = db_conn.prepare("SELECT name, label, owner,"
				" added, simfile, p.type, username, u.type"
			" FROM problems p, users u"
			" WHERE p.owner=u.id AND p.id=? AND p.type!=" PTYPE_VOID_STR);
		stmt.setString(1, problem_id_);

		MySQL::Result res = stmt.executeQuery();
		if (!res.next())
			return error404();

		problem_name = res[1];
		problem_label = res[2];
		problem_owner = res[3];
		problem_added = res[4];
		problem_simfile = res[5];
		problem_type = ProblemType(res.getUInt(6));
		owner_username = res[7];
		owner_utype = res.getUInt(8);

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return error500();
	}

	perms = getPermissions(problem_owner, problem_type);

	// Check permissions
	if (~perms & PERM_VIEW)
		return error403();

	StringView next_arg = url_args.extractNextArg();
	if (next_arg == "statement")
		return problemStatement(problem_id_);
	if (next_arg == "submissions")
		return problemSubmissions();
	if (next_arg == "edit")
		return editProblem();
	if (next_arg == "download")
		return downloadProblem();
	if (next_arg == "solutions")
		return problemSolutions();
	if (next_arg == "reupload")
		return reuploadProblem();
	if (next_arg == "rejudge")
		return rejudgeProblemSubmissions();
	if (next_arg == "delete")
		return deleteProblem();
	if (next_arg != "")
		return error404();

	// View the problem
	problemsetTemplate(StringBuff<40>("Problem ", problem_id_));

	append("<div class=\"right-flow\" style=\"width:90%;margin:12px 0\">");

	if (perms & PERM_DOWNLOAD)
		append("<a class=\"btn-small\" href=\"/p/", problem_id_, "/download\">"
				"Download problem</a>");

	if (perms & PERM_ADMIN)
		append("<a class=\"btn-small blue\" href=\"/p/", problem_id_, "/edit\">"
				"Edit problem</a>"
			"<a class=\"btn-small orange\" href=\"/p/", problem_id_,
				"/reupload\">Reupload the package</a>"
			"<a class=\"btn-small blue\" onclick=\"rejudgeProblemSubmissions(",
				problem_id_, ")\">Rejudge all submissions</a>"
			"<a class=\"btn-small red\" href=\"/p/", problem_id_, "/delete\">"
				"Delete problem</a>");

	append("</div>"
		"<div class=\"problem-info\">"
			"<div class=\"name\">"
				"<label>Name</label>",
				htmlEscape(problem_name),
			"</div>"
			"<div class=\"label\">"
				"<label>Label</label>",
				htmlEscape(problem_label),
			"</div>");

	if (perms & PERM_SEE_OWNER)
			append("<div class=\"owner\">"
				"<label>Owner</label>"
				"<a href=\"/u/", problem_owner, "\">", owner_username, "</a>"
			"</div>");

	append("<div class=\"added\">"
				"<label>Added</label>"
				"<span datetime=\"", problem_added, "\">", problem_added,
					"<sup>UTC</sup></span>"
			"</div>"
			"<div class=\"is_public\">"
				"<label>Is public</label>",
				(problem_type == ProblemType::PUBLIC ? "Yes" : "No"),
			"</div>"
		"</div>"
		"<center>"
			"<a class=\"btn\" href=\"/p/", problem_id_, "/statement\">"
				"View statement</a>"
		"</center>");

	// TODO: list files and allow to download/reupload them
	if (perms & PERM_SEE_SIMFILE)
		append("<h2>Problem's Simfile:</h2>"
		"<pre class=\"simfile\">", htmlEscape(problem_simfile), "</pre>");
}

void Problemset::editProblem() {
	if (~perms & PERM_ADMIN)
		return error403();

	error501();
}

void Problemset::downloadProblem() {
	if (~perms & PERM_DOWNLOAD)
		return error403();

	resp.content_type = server::HttpResponse::FILE;
	resp.headers["Content-Disposition"] = concat("attachment; filename=",
		problem_id_, ".zip");
	resp.content = concat("problems/", problem_id_, ".zip");
}

void Problemset::reuploadProblem() {
	if (~perms & PERM_ADMIN)
		return error403();

	FormValidator fv(req->form_data);
	string name, label, memory_limit, user_package_file, global_time_limit;
	bool force_auto_limit = true, ignore_simfile = false;

	if (req->method == server::HttpRequest::POST) {
		if (fv.get("csrf_token") != Session::csrf_token)
			return error403();

		string package_file;

		// Validate all fields
		fv.validate(name, "name", "Problem's name", PROBLEM_NAME_MAX_LEN);

		fv.validate(label, "label", "Problem's label", PROBLEM_LABEL_MAX_LEN);

		fv.validate(memory_limit, "memory-limit", "Memory limit",
			isDigitNotGreaterThan<(std::numeric_limits<uint64_t>::max() >> 20)>,
			"Memory limit: invalid value");

		fv.validate<bool(StringView)>(global_time_limit,
			"global-time-limit", "Global time limit", isReal,
			"Global time limit: invalid value"); // TODO: add length limit
		// Global time limit in usec
		uint64_t gtl =
			round(strtod(global_time_limit.c_str(), nullptr) * 1'000'000);
		if (global_time_limit.size() && gtl < 400000)
			fv.addError("Global time limit cannot be lower than 0.4 s");

		force_auto_limit = fv.exist("force-auto-limit");

		ignore_simfile = fv.exist("ignore-simfile");

		fv.validateNotBlank(user_package_file, "package", "Package");

		fv.validateFilePathNotEmpty(package_file, "package", "Package");

		// If all fields are OK
		if (fv.noErrors())
			try {
				jobs::AddProblemInfo ap_info {
					name,
					label,
					strtoull(memory_limit.c_str()),
					gtl,
					force_auto_limit,
					ignore_simfile,
					problem_type == ProblemType::PUBLIC,
					-1 // It will be set by the job server; it is done this way
					   // to avoid collisions with the problem editing jobs (not
					   // to dismiss one changing the problem's owner)
				};

				MySQL::Statement stmt {db_conn.prepare(
					"INSERT job_queue (creator, priority, type, status, added,"
						" info, data, aux_id) "
					"VALUES(?, ?, " JQTYPE_VOID_STR ", " JQSTATUS_PENDING_STR
						", ?, ?, '', ?)")};
				stmt.setString(1, Session::user_id);
				stmt.setUInt(2, priority(JobQueueType::REUPLOAD_PROBLEM));
				stmt.setString(3, date());
				stmt.setString(4, ap_info.dump());
				stmt.setString(5, problem_id_);
				stmt.executeUpdate();

				string jobid = db_conn.lastInsertId();

				// Move package file that it will become a job's file
				{
					StringBuff<PATH_MAX> new_path
						{"jobs_files/", jobid, ".zip"};
					if (::move(package_file, new_path))
						THROW("Error: link(`", package_file, "`, `", new_path,
							"`)", error(errno));

					package_file = new_path.data();
				}

				// Activate the job
				int rc = db_conn.executeUpdate("UPDATE job_queue SET type="
					JQTYPE_REUPLOAD_PROBLEM_STR " WHERE id=" + jobid);
				if (1 != rc)
					THROW("Failed to update");

				notifyJobServer();
				return redirect(concat("/jobs/", jobid));

			} catch (const std::exception& e) {
				ERRLOG_CATCH(e);
				fv.addError("Internal server error");
			}

	} else {
		name = problem_name;
		label = problem_label;

		// Load the global memory limit form the Simfile
		sim::Simfile sf {std::move(problem_simfile)};
		sf.loadGlobalMemoryLimitOnly();
		if (sf.global_mem_limit)
			memory_limit = toStr(sf.global_mem_limit >> 20).str;
	}

	problemsetTemplate("Reupload the problem " + problem_id_);
	append(fv.errors(), "<div class=\"form-container\">"
			"<h1>Reupload the problem<br/><a href=\"/p/", problem_id_, "\">",
				htmlEscape(name), "</a></h1>"
			"<form method=\"post\" enctype=\"multipart/form-data\">"
				// Name
				"<div class=\"field-group\">"
					"<label>Problem's name</label>"
					"<input type=\"text\" name=\"name\" value=\"",
						htmlEscape(name), "\" size=\"24\""
					"maxlength=\"", PROBLEM_NAME_MAX_LEN, "\" "
					"placeholder=\"Detect from Simfile\">"
				"</div>"
				// Label
				"<div class=\"field-group\">"
					"<label>Problem's label</label>"
					"<input type=\"text\" name=\"label\" value=\"",
						htmlEscape(label), "\" size=\"24\""
					"maxlength=\"", PROBLEM_LABEL_MAX_LEN, "\" "
					"placeholder=\"Detect from Simfile or from name\">"
				"</div>"
				// Memory limit
				"<div class=\"field-group\">"
					"<label>Memory limit [MB]</label>"
					"<input type=\"text\" name=\"memory-limit\" value=\"",
						htmlEscape(memory_limit), "\" size=\"24\" "
					"placeholder=\"Detect from Simfile\">"
				"</div>"
				// Global time limit
				"<div class=\"field-group\">"
					"<label>Global time limit [s] (for each test)</label>"
					"<input type=\"text\" name=\"global-time-limit\" value=\"",
						htmlEscape(global_time_limit), "\" size=\"24\" "
					"placeholder=\"No global time limit\">"
				"</div>"
				// Force auto limit
				"<div class=\"field-group\">"
					"<label>Force automatic time limit setting</label>"
					"<input type=\"checkbox\" name=\"force-auto-limit\"",
						(force_auto_limit ? " checked" : ""), ">"
				"</div>"
				// Ignore Simfile
				"<div class=\"field-group\">"
					"<label>Ignore Simfile</label>"
					"<input type=\"checkbox\" name=\"ignore-simfile\"",
						(ignore_simfile ? " checked" : ""), ">"
				"</div>"
				// Package
				"<div class=\"field-group\">"
					"<label>Package [ZIP file]</label>"
					"<input type=\"file\" name=\"package\" required>"
				"</div>"
				"<input class=\"btn blue\" type=\"submit\" value=\"Submit\">"
			"</form>"
		"</div>");
}

void Problemset::rejudgeProblemSubmissions() {
	if (~perms & PERM_ADMIN)
		return error403();

	FormValidator fv(req->form_data);
	if (req->method != server::HttpRequest::POST ||
		fv.get("csrf_token") != Session::csrf_token)
	{
		return error403();
	}

	try {
		MySQL::Statement stmt = db_conn.prepare("UPDATE submissions "
			"SET status=" SSTATUS_PENDING_STR " WHERE problem_id=?");
		stmt.setString(1, problem_id_);
		stmt.executeUpdate();

		// Add jobs to rejudge the submissions
		stmt = db_conn.prepare("INSERT job_queue (creator, status,"
				" priority, type, added, aux_id, info, data)"
			"SELECT ?, " JQSTATUS_PENDING_STR ", ?, ?, ?, id, ?, ''"
			" FROM submissions WHERE problem_id=? ORDER BY id");
		stmt.setString(1, Session::user_id);
		stmt.setUInt(2, priority(JobQueueType::JUDGE_SUBMISSION));
		stmt.setUInt(3, (uint)JobQueueType::JUDGE_SUBMISSION);
		stmt.setString(4, date());
		stmt.setString(5, jobs::dumpString(problem_id_));
		stmt.setString(6, problem_id_);
		stmt.executeUpdate();

		notifyJobServer();

		return response("200 OK");

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
	}
}

void Problemset::deleteProblem() {
	if (~perms & PERM_ADMIN)
		return error403();

	error501();
}

void Problemset::problemSolutions() {
	if (~perms & PERM_VIEW_SOLUTIONS)
		return error403();

	problemsetTemplate("Solutions to the problem " + problem_id_);
	append("<h1>Solutions to the problem <a href=\"/p/", problem_id_, "\">",
		htmlEscape(problem_name), "</a></h1>");
	try {
		MySQL::Statement stmt {db_conn.prepare(
			"SELECT id, submit_time, status, score FROM submissions"
			" WHERE type=" STYPE_PROBLEM_SOLUTION_STR " AND problem_id=?"
			" ORDER BY id DESC")};
		stmt.setString(1, problem_id_);

		MySQL::Result res {stmt.executeQuery()};
		append("<table class=\"submissions\">"
			"<thead>"
				"<tr>",
					"<th class=\"time\">Submission time<sup>UTC</sup></th>"
					"<th class=\"status\">Status</th>"
					"<th class=\"score\">Score</th>"
					"<th class=\"actions\">Actions</th>"
				"</tr>"
			"</thead>"
			"<tbody>");

		while (res.next()) {
			append("<td><a href=\"/s/", res[1], "\" datetime=\"", res[2], "\">",
						res[2], "</a></td>",
					submissionStatusAsTd(SubmissionStatus(res.getUInt(3)),
						true),
					"<td>", res[4], "</td>"
					"<td>"
						"<a class=\"btn-small\" href=\"/s/", res[1],
							"\">View</a>"
						"<a class=\"btn-small\" href=\"/s/", res[1],
							"/source\">Source</a>"
						"<a class=\"btn-small\" href=\"/s/", res[1],
							"/download\">Download</a>"
					"<a class=\"btn-small blue\" onclick=\"rejudgeSubmission(",
						res[1], ")\">Rejudge</a>"
					"</td>"
				"<tr>");
		}

		append("</tbody>"
			"</table>");

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return error500();
	}
}

void Problemset::deleteSubmission(const string& submission_id,
	const string& submission_owner)
{
	return error501();
}

void Problemset::changeSubmissionTypeTo(const string& submission_id,
	const string& submission_owner, SubmissionType stype)
{
	return error501();
}

void Problemset::problemSubmissions() {
	if (~perms & PERM_VIEW)
		return error403();

	if (!Session::isOpen())
		return redirect("/login?" + req->target);

	error501();
}

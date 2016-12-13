#include "problemset.h"

#include <simlib/config_file.h>
#include <simlib/time.h>

using std::string;

void Problemset::problemsetTemplate(const StringView& title,
	const StringView& styles, const StringView& scripts)
{
	baseTemplate(title, concat("body{margin-left:190px}", styles),
		scripts);
	if (!Session::isOpen())
		return;

	append("<ul class=\"menu\">"
			"<span>PROBLEMSET</span>"
			"<a href=\"/p/add\">Add a problem</a>"
			"<hr>"
			"<a href=\"/p\">All problems</a>"
			"<a href=\"/p/my\">My problems</a>");
	if (!problem_id_.empty()) {
		append("<hr/>"
			"<a href=\"/p/", problem_id_, "\">View problem</a>"
			"<a href=\"/p/", problem_id_, "/edit\">Edit problem</a>"
			"<a href=\"/p/", problem_id_, "/statement\">View statement</a>"
			"<a href=\"/p/", problem_id_, "/solutions\">Solutions</a>");
	}
	append("</ul>");
}

void Problemset::handle() {
	if (!Session::open())
		return redirect("/login?" + req->target);

	if (Session::user_type > UTYPE_TEACHER)
		return error403();

	StringView next_arg = url_args.extractNextArg();
	if (isDigit(next_arg)) {
		problem_id_ = next_arg.to_string();
		return problem();
	}
	problem_id_.clear();

	if (next_arg == "add")
		return addProblem();

	if (!next_arg.empty() && next_arg != "my")
		return error404();

	problemsetTemplate("Problemset");
	append(next_arg.empty() ? "<h1>Problemset</h1>" : "<h1>My problems</h1>");
	append("<a class=\"btn\" href=\"/p/add\">Add a problem</a>");

	// List available problems
	try {
		DB::Statement stmt;
		if (next_arg == "my") {
			stmt = db_conn.prepare("SELECT id, name, label, added, is_public"
				" FROM problems WHERE owner=?");
			stmt.setString(1, Session::user_id);

		} else if (Session::user_type == UTYPE_ADMIN) {
			stmt = db_conn.prepare("SELECT p.id, name, label, added, is_public,"
					" owner, u.username, u.type FROM problems p, users u"
				" WHERE owner=u.id");

		} else {
			stmt = db_conn.prepare("SELECT p.id, name, label, added, is_public,"
					" owner, u.username, u.type FROM problems p, users u"
				" WHERE owner=u.id AND (is_public=1 OR owner=?)");
			stmt.setString(1, Session::user_id);
		}

		DB::Result res = stmt.executeQuery();
		if (!res.next()) {
			append("There are no problems to show...");
			return;
		}

		append("<table class=\"problems\">"
			"<thead>"
				"<tr>"
					"<th class=\"id\">Id</th>"
					"<th class=\"name\">Name</th>"
					"<th class=\"label\">Label</th>",
					(next_arg.empty() ? "<th class=\"owner\">Owner</th>" : ""),
					"<th class=\"added\">Added<sup>UTC+0</sup></th>"
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

			if (next_arg.empty()) // Owner
				append("<td><a href=\"/u/", res[6], "\">", res[7], "</a></td>");

			append("<td datetime=\"", res[4], "\">", res[4], "</td>"
					"<td>", (res.getBool(5) ? "YES" : "NO"), "</td>"
					"<td>"
						"<a class=\"btn-small\" href=\"/p/", res[1], "\">"
							"View</a>"
						"<a class=\"btn-small\" href=\"/p/", res[1],
							"/statement\">Statement</a>"
						"<a class=\"btn-small\" href=\"/p/", res[1],
							"/solutions\">Solutions</a>");

			// More actions
			if (Session::user_type == UTYPE_ADMIN ||
				Session::user_id == res[6] ||
				Session::user_type < res.getUInt(8))
			{
				append("<a class=\"btn-small blue\" href=\"/p/", res[1],
					"/edit\">Edit</a>");
			}

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
		DB::Statement stmt =
			db_conn.prepare("SELECT simfile FROM problems WHERE id=?");
		stmt.setString(1, problem_id.to_string());

		DB::Result res = stmt.executeQuery();
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
		concat("inline; filename=", filename(statement));

	resp.content_type = server::HttpResponse::FILE;
	resp.content = concat("problems/", problem_id, '/', statement);
}

void Problemset::addProblem() {
	error501();
}

void Problemset::reuploadProblem() {
	error501();
}

void Problemset::problem() {
	// Fetch problem info
	try {
		DB::Statement stmt = db_conn.prepare("SELECT name, label, owner, added,"
				" simfile, is_public, username, type"
			" FROM problems p, users u WHERE p.owner=u.id AND p.id=?");
		stmt.setString(1, problem_id_);

		DB::Result res = stmt.executeQuery();
		if (!res.next())
			return error404();

		problem_name = res[1];
		problem_label = res[2];
		problem_owner = res[3];
		problem_added = res[4];
		problem_simfile = res[5];
		problem_is_public = res.getBool(6);
		owner_username = res[7];
		owner_utype = res.getUInt(8);

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return error500();
	}

	// Check permissions
	if (!problem_is_public && Session::user_type != UTYPE_ADMIN &&
		Session::user_id != problem_owner && Session::user_type >= owner_utype)
	{
		return error403();
	}

	StringView next_arg = url_args.extractNextArg();
	if (next_arg == "statement")
		return problemStatement(problem_id_);
	if (next_arg == "edit")
		return editProblem();
	if (next_arg == "reupload")
		return reuploadProblem();
	if (next_arg == "solutions")
		return problemSolutions();
	if (next_arg == "delete")
		return deleteProblem();
	if (next_arg != "")
		return error404();

	// View the problem
	problemsetTemplate(StringBuff<40>("Problem ", problem_id_));

	append("<div class=\"right-flow\" style=\"width:90%;margin:12px 0\">"
			"<a class=\"btn-small\" href=\"/p/", problem_id_, "/statement\">"
				"View statement</a>"
			"<a class=\"btn-small blue\" href=\"/p/", problem_id_, "/edit\">"
				"Edit problem</a>"
			"<a class=\"btn-small orange\" href=\"/p/", problem_id_,
				"/reupload\">Reupload the package</a>"
			"<a class=\"btn-small red\" href=\"/p/", problem_id_, "/delete\">"
				"Delete problem</a>"
		"</div>"
		"<div class=\"problem-info\">"
			"<div class=\"name\">"
				"<label>Name</label>",
				htmlEscape(problem_name),
			"</div>"
			"<div class=\"label\">"
				"<label>Label</label>",
				htmlEscape(problem_label),
			"</div>"
			"<div class=\"owner\">"
				"<label>Owner</label>"
				"<a href=\"/u/", problem_owner, "\">", owner_username, "</a>"
			"</div>"
			"<div class=\"added\">"
				"<label>Added</label>"
				"<span datetime=\"", problem_added, "\">", problem_added,
					"<sup>UTC+0</sup></span>"
			"</div>"
			"<div class=\"is_public\">"
				"<label>Is public</label>",
				(problem_is_public ? "Yes" : "No"),
			"</div>"
		"</div>"
		"<h2>Problem's Simfile:</h2>"
		"<pre class=\"simfile\">", htmlEscape(problem_simfile), "</pre>");
}

void Problemset::editProblem() {
	error501();
}

void Problemset::deleteProblem() {
	error501();
}

void Problemset::problemSolutions() {
	error501();
}

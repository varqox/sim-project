#include "contest.h"

#include <simlib/debug.h>
#include <simlib/logger.h>
#include <simlib/time.h>

using std::string;
using std::vector;

Contest::RoundPath* Contest::getRoundPath(const string& round_id) {
	std::unique_ptr<RoundPath> r_path(new RoundPath(round_id));

	try {
		DB::Statement stmt = db_conn.prepare(
			"SELECT id, parent, problem_id, name, owner, is_public, visible, "
				"show_ranking, begins, ends, full_results "
			"FROM rounds "
			"WHERE id=? OR id=(SELECT parent FROM rounds WHERE id=?) OR "
				"id=(SELECT grandparent FROM rounds WHERE id=?)");
		stmt.setString(1, round_id);
		stmt.setString(2, round_id);
		stmt.setString(3, round_id);

		DB::Result res = stmt.executeQuery();

		auto copyDataTo = [&](std::unique_ptr<Round>& r) {
			r.reset(new Round);
			r->id = res[1];
			r->parent = res[2];
			r->problem_id = res[3];
			r->name = res[4];
			r->owner = res[5];
			r->is_public = res.getBool(6);
			r->visible = res.getBool(7);
			r->show_ranking = res.getBool(8);
			r->begins = res[9];
			r->ends = res[10];
			r->full_results = res[11];
		};

		int rows = res.rowCount();
		// If round does not exist
		if (rows == 0) {
			error404();
			return nullptr;
		}

		r_path->type = (rows == 1 ? CONTEST : (rows == 2 ? ROUND : PROBLEM));

		while (res.next()) {
			if (res.isNull(2))
				copyDataTo(r_path->contest);
			else if (res.isNull(3)) // problem_id IS NULL
				copyDataTo(r_path->round);
			else
				copyDataTo(r_path->problem);
		}

		// Check rounds hierarchy
		if (!r_path->contest || (rows > 1 && !r_path->round)
			|| (rows > 2 && !r_path->problem))
		{
			THROW("Database error: corrupted hierarchy of rounds (id: ",
				round_id, ")");
		}

		/* Check access */

		r_path->admin_access = isAdmin(*r_path); // TODO: get data in above query!
		if (r_path->admin_access)
			return r_path.release();

		/* admin_access == false */

		if (!r_path->contest->is_public) {
			// Check access to contest
			if (!Session::open()) {
				redirect(concat("/login?", req->target));
				return nullptr;
			}

			stmt = db_conn.prepare("SELECT user_id "
				"FROM users_to_contests WHERE user_id=? AND contest_id=?");
			stmt.setString(1, Session::user_id);
			stmt.setString(2, r_path->contest->id);

			res = stmt.executeQuery();
			if (!res.next()) {
				// User is not assigned to this contest
				error403();
				return nullptr;
			}
		}

		// Check access to round - check if round has begun
		// GRANT ACCESS if and only if:
		// 1) type == CONTEST
		// 2) type == ROUND and round has begun or is visible
		// 3) type == PROBLEM and parent round has begun
		if (r_path->type == CONTEST || // 1
			r_path->round->begins.empty() || // 2 & 3
			r_path->round->begins <= date("%Y-%m-%d %H:%M:%S") || // 2 & 3
			(r_path->type == ROUND && r_path->round->visible)) // 2 \ 3
		{
			return r_path.release();
		}

		// Otherwise error 403
		error403();
		return nullptr;

	} catch (const std::exception& e) {
		ERRLOG_CAUGHT(e);
		error500();
		return nullptr;
	}
}

Template::TemplateEnder Contest::contestTemplate(const StringView& title,
	const StringView& styles, const StringView& scripts)
{

	auto ender = baseTemplate(title, concat(".body{margin-left:190px}", styles),
		scripts);

	append("<ul class=\"menu\">\n");

	// Aliases
	string &round_id = rpath->round_id;
	bool &admin_access = rpath->admin_access;

	if (admin_access) {
		append("<span>CONTEST ADMINISTRATION</span>\n"
			// Adding
			"<div class=\"dropmenu right\">"
				"<a class=\"dropmenu-toggle\">Add</a>\n"
				"<ul>\n"
					"<a href=\"/c/add\">Contest</a>\n"
					"<a href=\"/c/", rpath->contest->id, "/add\">Round</a>\n");

		if (rpath->type >= ROUND)
			append("<a href=\"/c/", rpath->round->id, "/add\">Problem</a>\n");

		append("</ul>\n"
			"</div>\n"

		// Editing
			"<div class=\"dropmenu right\">"
				"<a class=\"dropmenu-toggle\">Edit</a>\n"
			"<ul>\n"
				"<a href=\"/c/", rpath->contest->id, "/edit\">Contest</a>\n");

		if (rpath->type >= ROUND)
			append("<a href=\"/c/", rpath->round->id, "/edit\">Round</a>\n");
		if (rpath->type == PROBLEM)
			append("<a href=\"/c/", round_id, "/edit\">Problem</a>\n");

		append("</ul>\n"
			"</div>\n"
			"<hr/>\n"
			"<a href=\"/c/", rpath->contest->id, "\">Contest dashboard</a>\n"
			"<a href=\"/c/", rpath->contest->id, "/problems\">Problems</a>\n"
			"<a href=\"/c/", rpath->contest->id, "/files\">Files</a>\n"
			"<a href=\"/c/", round_id, "/submit\">Submit a solution</a>\n"
			"<a href=\"/c/", rpath->contest->id, "/submissions\">"
				"Submissions</a>\n"
			"<a href=\"/c/", round_id, "/submissions\">Local submissions</a>\n"
			"<a href=\"/c/", rpath->contest->id, "/ranking\">Ranking</a>\n"
			"<span>OBSERVER MENU</span>\n");
	}

	append("<a href=\"/c/", rpath->contest->id, (admin_access ? "/n" : ""),
			"\">Contest dashboard</a>\n"
		"<a href=\"/c/", rpath->contest->id, (admin_access ? "/n" : ""),
			"/problems\">Problems</a>\n"
		"<a href=\"/c/", rpath->contest->id, (admin_access ? "/n" : ""),
			"/files\">Files</a>\n");

	string current_date = date("%Y-%m-%d %H:%M:%S");
	Round *round = rpath->round.get();
	if (rpath->type == CONTEST || (
		(round->begins.empty() || round->begins <= current_date) &&
		(round->ends.empty() || current_date < round->ends)))
	{
		append("<a href=\"/c/", round_id, (admin_access ? "/n" : ""),
			"/submit\">Submit a solution</a>\n");
	}

	append("<a href=\"/c/", rpath->contest->id,
		(admin_access ? "/n" : ""), "/submissions\">Submissions</a>\n");

	append("<a href=\"/c/", round_id, (admin_access ? "/n" : ""),
		"/submissions\">Local submissions</a>\n");

	if (rpath->contest->show_ranking)
		append("<a href=\"/c/", rpath->contest->id,
			(admin_access ? "/n" : ""), "/ranking\">Ranking</a>\n");

	append("</ul>");

	return ender;
}

bool Contest::isAdmin(const RoundPath& r_path) {
	// If is not logged in, he cannot be admin
	if (!Session::open())
		return false;

	// User is the owner of the contest or is the Sim root
	if (r_path.contest->owner == Session::user_id
		|| Session::user_id == SIM_ROOT_UID)
	{
		return true;
	}

	try {
		// Check if user has more privileges than the owner
		DB::Statement stmt = db_conn.prepare(
			"SELECT type FROM users WHERE id=?");
		stmt.setString(1, r_path.contest->owner);

		DB::Result res = stmt.executeQuery();
		int owner_type = UTYPE_ADMIN;
		if (res.next())
			owner_type = res.getUInt(1);

		return owner_type > Session::user_type;
		// TODO: give SIM root privileges to admins' contests (in contest list
		// in handle() also)

	} catch (const std::exception& e) {
		ERRLOG_CAUGHT(e);
	}

	return false;
}

void Contest::printRoundPath(const StringView& page, bool force_normal) {
	append("<div class=\"round-path\"><a href=\"/c/", rpath->contest->id,
		(force_normal ? "/n/" : "/"), page, "\">",
		htmlSpecialChars(rpath->contest->name), "</a>");

	if (rpath->type != CONTEST) {
		append(" ~~> <a href=\"/c/", rpath->round->id,
			(force_normal ? "/n/" : "/"), page, "\">",
			htmlSpecialChars(rpath->round->name), "</a>");

		if (rpath->type == PROBLEM)
			append(" ~~> <a href=\"/c/", rpath->problem->id,
				(force_normal ? "/n/" : "/"), page, "\">",
				htmlSpecialChars(rpath->problem->name), "</a>");
	}

	append("</div>\n");
}

void Contest::printRoundView(bool link_to_problem_statement, bool admin_view) {
	const char* force_normal = (!admin_view && rpath->admin_access ? "/n" : "");
	try {
		if (rpath->type == CONTEST) {
			// Select subrounds
			DB::Statement stmt = db_conn.prepare(admin_view ?
					"SELECT id, name, item, visible, begins, ends, "
						"full_results FROM rounds WHERE parent=? ORDER BY item"
				: "SELECT id, name, item, visible, begins, ends, full_results "
					"FROM rounds WHERE parent=? AND "
						"(visible IS TRUE OR begins IS NULL OR begins<=?) "
					"ORDER BY item");
			stmt.setString(1, rpath->contest->id);
			string current_date;
			if (!admin_view)
				stmt.setString(2, (current_date = date("%Y-%m-%d %H:%M:%S")));

			struct SubroundExtended {
				string id, name, item, begins, ends, full_results;
				bool visible;
			};

			DB::Result res = stmt.executeQuery();
			vector<SubroundExtended> subrounds;
			// For performance
			subrounds.reserve(res.rowCount());

			// Collect results
			while (res.next()) {
				subrounds.emplace_back();
				subrounds.back().id = res[1];
				subrounds.back().name = res[2];
				subrounds.back().item = res[3];
				subrounds.back().visible = res.getBool(4);
				subrounds.back().begins = res[5];
				subrounds.back().ends = res[6];
				subrounds.back().full_results = res[7];
			}

			// Select problems
			stmt = db_conn.prepare("SELECT id, parent, name FROM rounds "
					"WHERE grandparent=? ORDER BY item");
			stmt.setString(1, rpath->contest->id);

			res = stmt.executeQuery();
			std::map<string, vector<Problem> > problems; // (round_id, problems)

			// Fill with all subrounds, which has begun <- for these rounds
			// problems will be listed
			if (admin_view)
				for (auto&& sr : subrounds)
					problems[sr.id];
			else
				for (auto&& sr : subrounds)
					if (sr.begins.empty() || sr.begins <= current_date)
						problems[sr.id];

			// Collect results
			while (res.next()) {
				// Get reference to proper vector<Problem>
				auto it = problems.find(res[2]);
				// Database error or problem parent round has not began yet
				if (it == problems.end())
					continue; // Ignore

				vector<Problem>& prob = it->second;
				prob.emplace_back();
				prob.back().id = res[1];
				prob.back().parent = res[2];
				prob.back().name = res[3];
			}

			// Construct "table"
			append("<div class=\"round-view\">\n"
				"<a class=\"grayed\" href=\"/c/", rpath->contest->id,
					force_normal, "\">", htmlSpecialChars(rpath->contest->name),
				"</a>\n"
				"<div>\n");

			// For each subround list all problems
			for (auto&& sr : subrounds) {
				// Round
				append("<div>\n"
					"<a href=\"/c/", sr.id, force_normal, "\">",
					htmlSpecialChars(sr.name), "</a>\n");

				// List problems
				vector<Problem>& prob = problems[sr.id];
				for (auto&& pro : prob) {
					append("<a href=\"/c/", pro.id, force_normal);

					if (link_to_problem_statement)
						append("/statement");

					append("\">", htmlSpecialChars(pro.name), "</a>\n");
				}
				append("</div>\n");
			}
			append("</div>\n"
				"</div>\n");

		} else if (rpath->type == ROUND) {
			// Construct "table"
			append("<div class=\"round-view\">\n"
				"<a href=\"/c/", rpath->contest->id, force_normal, "\">",
					htmlSpecialChars(rpath->contest->name), "</a>\n"
				"<div>\n");
			// Round
			append("<div>\n"
				"<a class=\"grayed\" href=\"/c/", rpath->round->id,
					force_normal, "\">", htmlSpecialChars(rpath->round->name),
				"</a>\n");

			// List problems if and only if round has begun (for non-admins)
			if (admin_view ||
				rpath->round->begins.empty() ||
				rpath->round->begins <= date("%Y-%m-%d %H:%M:%S"))
			{
				// Select problems
				DB::Statement stmt = db_conn.prepare("SELECT id, name "
					"FROM rounds WHERE parent=? ORDER BY item");
				stmt.setString(1, rpath->round->id);

				// List problems
				DB::Result res = stmt.executeQuery();
				while (res.next()) {
					append("<a href=\"/c/", res[1], force_normal);

					if (link_to_problem_statement)
						append("/statement");

					append("\">", htmlSpecialChars(res[2]), "</a>\n");
				}
			}

			append("</div>\n"
				"</div>\n"
				"</div>\n");

		} else { // rpath->type == PROBLEM
			// Construct "table"
			append("<div class=\"round-view\">\n"
				"<a href=\"/c/", rpath->contest->id, force_normal, "\">",
					htmlSpecialChars(rpath->contest->name), "</a>\n"
				"<div>\n");
			// Round
			append("<div>\n"
				"<a href=\"/c/", rpath->round->id, force_normal, "\">",
					htmlSpecialChars(rpath->round->name), "</a>\n"
			// Problem
				"<a class=\"grayed\" href=\"/c/", rpath->problem->id,
					force_normal);

			if (link_to_problem_statement)
				append("/statement");

			append("\">", htmlSpecialChars(rpath->problem->name), "</a>\n"
					"</div>\n"
				"</div>\n"
				"</div>\n");
		}

	} catch (const std::exception& e) {
		ERRLOG_CAUGHT(e);
	}
}

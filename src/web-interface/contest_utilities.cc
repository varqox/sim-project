#include <simlib/debug.h>
#include <simlib/time.h>

using std::pair;
using std::string;
using std::vector;

Contest::RoundPath* Contest::getRoundPath(const string& round_id) {
	std::unique_ptr<RoundPath> r_path(new RoundPath(round_id));

	try {
		MySQL::Statement stmt = db_conn.prepare(
			"SELECT id, parent, problem_id, name, owner, is_public, visible, "
				"show_ranking, begins, ends, full_results "
			"FROM rounds "
			"WHERE id=? OR id=(SELECT parent FROM rounds WHERE id=?) OR "
				"id=(SELECT grandparent FROM rounds WHERE id=?)");
		stmt.setString(1, round_id);
		stmt.setString(2, round_id);
		stmt.setString(3, round_id);

		MySQL::Result res = stmt.executeQuery();

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
				round_id, ')');
		}

		/* Check access */

		r_path->admin_access = false;
		if (Session::open()) {
			// User is the owner of the contest or is the Sim root
			if (r_path->contest->owner == Session::user_id
				|| Session::user_id == SIM_ROOT_UID)
			{
				r_path->admin_access = true;
				return r_path.release();
			}

			stmt = db_conn.prepare("SELECT type, mode "
				"FROM users,"
					"((SELECT mode FROM contests_users WHERE user_id=? "
							"AND contest_id=?) "
						"UNION (SELECT NULL AS mode) LIMIT 1) x "
					"WHERE id=?");
			stmt.setString(1, Session::user_id);
			stmt.setString(2, r_path->contest->id);
			stmt.setString(3, r_path->contest->owner);

			res = stmt.executeQuery();
			if (!res.next()) {
				error500();
				return nullptr;
			}

			// The user is more privileged than the contest owner
			if (res.getUInt(1) > Session::user_type) {
				r_path->admin_access = true;
				return r_path.release();
			}

			if (!res.isNull(2)) { // Row exist in contests_users
				uint cu_type = res.getUInt(2);
				if (cu_type == CU_MODE_MODERATOR) {
					r_path->admin_access = true;
					return r_path.release();
				}

			// The user is not on the 'white list'
			} else if (!r_path->contest->is_public) {
				redirect(concat("/login?", req->target));
				return nullptr;
			}

		// Session is not open
		} else if (!r_path->contest->is_public) {
			redirect(concat("/login?", req->target));
			return nullptr;
		}

		/* admin_access == false */

		// Check access to round - check if round has begun
		// GRANT ACCESS if and only if:
		// 1) type == CONTEST
		// 2) type == ROUND and round has begun or is visible
		// 3) type == PROBLEM and parent round has begun
		if (r_path->type == CONTEST || // 1
			r_path->round->begins <= date() || // 2 & 3
			(r_path->type == ROUND && r_path->round->visible)) // 2 \ 3
		{
			return r_path.release();
		}

		// Otherwise error 403
		error403();
		return nullptr;

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		error500();
		return nullptr;
	}
}

void Contest::contestTemplate(StringView title, StringView styles,
	StringView scripts)
{

	baseTemplate(title, concat("body{margin-left:190px}", styles),
		scripts);

	append("<ul class=\"menu\">");

	// Aliases
	string &round_id = rpath->round_id;
	bool &admin_access = rpath->admin_access;

	if (admin_access) {
		append("<span>CONTEST ADMINISTRATION</span>"
			// Adding
			"<div class=\"dropmenu right\">"
				"<a class=\"dropmenu-toggle\">Add</a>"
				"<ul>"
					"<a href=\"/c/add\">Contest</a>"
					"<a href=\"/c/", rpath->contest->id, "/add\">Round</a>");

		if (rpath->type >= ROUND)
			append("<a href=\"/c/", rpath->round->id, "/add\">Problem</a>");

		append("</ul>"
			"</div>"

		// Editing
			"<div class=\"dropmenu right\">"
				"<a class=\"dropmenu-toggle\">Edit</a>"
			"<ul>"
				"<a href=\"/c/", rpath->contest->id, "/edit\">Contest</a>");

		if (rpath->type >= ROUND)
			append("<a href=\"/c/", rpath->round->id, "/edit\">Round</a>");
		if (rpath->type == PROBLEM)
			append("<a href=\"/c/", round_id, "/edit\">Problem</a>");

		append("</ul>"
			"</div>"
			"<hr/>"
			"<a href=\"/c/", rpath->contest->id, "\">Contest dashboard</a>"
			"<a href=\"/c/", rpath->contest->id, "/users\">Users</a>"
			"<a href=\"/c/", rpath->contest->id, "/problems\">Problems</a>"
			"<a href=\"/c/", rpath->contest->id, "/files\">Files</a>"
			"<a href=\"/c/", round_id, "/submit\">Submit a solution</a>"
			"<a href=\"/c/", rpath->contest->id, "/submissions\">"
				"Submissions</a>"
			"<a href=\"/c/", round_id, "/submissions\">Local submissions</a>"
			"<a href=\"/c/", rpath->contest->id, "/ranking\">Ranking</a>"
			"<span>OBSERVER MENU</span>");
	}

	append("<a href=\"/c/", rpath->contest->id, (admin_access ? "/n" : ""),
			"\">Contest dashboard</a>"
		"<a href=\"/c/", rpath->contest->id, (admin_access ? "/n" : ""),
			"/problems\">Problems</a>"
		"<a href=\"/c/", rpath->contest->id, (admin_access ? "/n" : ""),
			"/files\">Files</a>");

	string current_date = date();
	Round *round = rpath->round.get();
	if (rpath->type == CONTEST || (round->begins <= current_date &&
		(round->ends.empty() || current_date < round->ends)))
	{
		append("<a href=\"/c/", round_id, (admin_access ? "/n" : ""),
			"/submit\">Submit a solution</a>");
	}

	append("<a href=\"/c/", rpath->contest->id,
		(admin_access ? "/n" : ""), "/submissions\">Submissions</a>");

	append("<a href=\"/c/", round_id, (admin_access ? "/n" : ""),
		"/submissions\">Local submissions</a>");

	if (rpath->contest->show_ranking)
		append("<a href=\"/c/", rpath->contest->id,
			(admin_access ? "/n" : ""), "/ranking\">Ranking</a>");

	append("</ul>");
}

void Contest::printRoundPath(StringView page, bool force_normal) {
	append("<div class=\"round-path\"><a href=\"/c/", rpath->contest->id,
		(force_normal ? "/n/" : "/"), page, "\">",
		htmlEscape(rpath->contest->name), "</a>");

	if (rpath->type != CONTEST) {
		append(" ~~> <a href=\"/c/", rpath->round->id,
			(force_normal ? "/n/" : "/"), page, "\">",
			htmlEscape(rpath->round->name), "</a>");

		if (rpath->type == PROBLEM)
			append(" ~~> <a href=\"/c/", rpath->round_id,
				(force_normal ? "/n/" : "/"), page, "\">",
				htmlEscape(rpath->problem->name), "</a>"
			"<a class=\"btn-small\" href=\"/c/", rpath->round_id,
				"/statement\">View statement</a>");
	}

	append("</div>");
}

void Contest::printRoundView(bool link_to_problem_statement, bool admin_view) {
	const char* force_normal = (!admin_view && rpath->admin_access ? "/n" : "");
	string current_date = date();

	auto round_duration = [](const string& begin, const string& end) {
		return concat("<div>"
				"<label>From: </label><span",
					(begin.empty() ? ">The Big Bang" : concat(" datetime=\"",
						begin, "\">", begin, "<sup>UTC</sup>")),
					"</span>"
			"</div>"
			"<div>"
				"<label>To: </label><span",
					(end.empty() ? ">Forever" : concat(" datetime=\"",
						end, "\">", end, "<sup>UTC</sup>")),
					"</span>"
			"</div>");
	};

	try {
		if (rpath->type == CONTEST) {
			// Select subrounds
			MySQL::Statement stmt = db_conn.prepare(admin_view ?
					"SELECT id, name, item, visible, begins, ends, "
						"full_results FROM rounds WHERE parent=? ORDER BY item"
				: "SELECT id, name, item, visible, begins, ends, full_results "
					"FROM rounds WHERE parent=? AND "
						"(visible IS TRUE OR begins IS NULL OR begins<=?) "
					"ORDER BY item");
			stmt.setString(1, rpath->contest->id);
			if (!admin_view)
				stmt.setString(2, current_date);

			struct SubroundExtended {
				string id, name, item, begins, ends, full_results;
				bool visible;
			};

			MySQL::Result res = stmt.executeQuery();
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
			std::map<string, vector<Problem>> problems; // (round_id, problems)

			// Fill with all subrounds, which has begun <- for these rounds
			// problems will be listed
			if (admin_view)
				for (auto&& sr : subrounds)
					problems[sr.id];
			else
				for (auto&& sr : subrounds)
					if (sr.begins <= current_date)
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

			// Collect user's final submissions to listed problems
			vector<pair<string, SubmissionStatus>> rid2stat; /*
			 	(round_id, status of the final submission) */
			if (Session::isOpen()) {
				stmt = db_conn.prepare(
					"SELECT round_id, status FROM submissions "
					"WHERE type=" STYPE_FINAL_STR " AND owner=? "
						"AND contest_round_id=?");
				stmt.setString(1, Session::user_id);
				stmt.setString(2, rpath->round_id);
				res = stmt.executeQuery();

				while (res.next())
					rid2stat.emplace_back(res[1],
						static_cast<SubmissionStatus>(res.getUInt(2)));

				sort(rid2stat);
			}

			// Construct "table"
			append("<div class=\"round-view\">"
				"<a class=\"grayed\" href=\"/c/", rpath->contest->id,
					force_normal, "\">", htmlEscape(rpath->contest->name),
				"</a>"
				"<div>");

			// For each subround list all problems
			for (auto&& sr : subrounds) {
				// Round
				append("<div>"
					"<a href=\"/c/", sr.id, force_normal, "\">",
						htmlEscape(sr.name),
						round_duration(sr.begins, sr.ends),
					"</a>");

				bool show_full_results = (admin_view ||
					sr.full_results <= current_date);

				// List problems
				vector<Problem>& prob = problems[sr.id];
				for (auto&& pro : prob) {
					append("<a");

					// Try to find the final submission to this problem
					auto it = binaryFindBy(rid2stat,
						&decltype(rid2stat)::value_type::first, pro.id);
					if (it != rid2stat.end())
						append(" class=\"", submissionStatusCSSClass(it->second,
							show_full_results), '"');

					append(" href=\"/c/", pro.id, force_normal);

					if (link_to_problem_statement)
						append("/statement");

					append("\">", htmlEscape(pro.name), "</a>");
				}
				append("</div>");
			}
			append("</div>"
				"</div>");

		} else if (rpath->type == ROUND) {
			// Construct "table"
			append("<div class=\"round-view\">"
				"<a href=\"/c/", rpath->contest->id, force_normal, "\">",
					htmlEscape(rpath->contest->name), "</a>"
				"<div>");
			// Round
			append("<div>"
				"<a class=\"grayed\" href=\"/c/", rpath->round->id,
					force_normal, "\">", htmlEscape(rpath->round->name),
					round_duration(rpath->round->begins, rpath->round->ends),
				"</a>");

			// List problems if and only if the round has begun (for non-admins)
			if (admin_view || rpath->round->begins <= date())
			{
				// Collect user's final submissions to the listed problems
				vector<pair<string, SubmissionStatus>> rid2stat; /*
				 	(round_id, status of the final submission) */
				if (Session::isOpen()) {
					MySQL::Statement stmt = db_conn.prepare(
						"SELECT round_id, status FROM submissions "
						"WHERE type=" STYPE_FINAL_STR " AND owner=? "
							"AND parent_round_id=?");
					stmt.setString(1, Session::user_id);
					stmt.setString(2, rpath->round_id);
					MySQL::Result res = stmt.executeQuery();

					while (res.next())
						rid2stat.emplace_back(res[1],
							static_cast<SubmissionStatus>(res.getUInt(2)));

					sort(rid2stat);
				}

				bool show_full_results = (admin_view ||
					rpath->round->full_results <= current_date);

				// Select problems
				MySQL::Statement stmt = db_conn.prepare("SELECT id, name "
					"FROM rounds WHERE parent=? ORDER BY item");
				stmt.setString(1, rpath->round->id);

				// List problems
				MySQL::Result res = stmt.executeQuery();
				while (res.next()) {
					append("<a");

					// Try to find the final submission to this problem
					auto it = binaryFindBy(rid2stat,
						&decltype(rid2stat)::value_type::first, res[1]);
					if (it != rid2stat.end())
						append(" class=\"", submissionStatusCSSClass(it->second,
							show_full_results), '"');

					append(" href=\"/c/", res[1], force_normal);

					if (link_to_problem_statement)
						append("/statement");

					append("\">", htmlEscape(res[2]), "</a>");
				}
			}

			append("</div>"
				"</div>"
				"</div>");

		} else { // rpath->type == PROBLEM
			string status_classes;
			bool show_full_results = (admin_view ||
				rpath->round->full_results <= current_date);

			// Get status of the final submission to the problem
			if (Session::isOpen()) {
				MySQL::Statement stmt = db_conn.prepare(
					"SELECT status FROM submissions "
					"WHERE type=" STYPE_FINAL_STR " AND owner=? "
						"AND round_id=?");
				stmt.setString(1, Session::user_id);
				stmt.setString(2, rpath->round_id);
				MySQL::Result res = stmt.executeQuery();

				if (res.next())
					status_classes = concat(' ', submissionStatusCSSClass(
						static_cast<SubmissionStatus>(res.getUInt(1)),
						show_full_results));
			}

			// Construct "table"
			append("<div class=\"round-view\">"
				"<a href=\"/c/", rpath->contest->id, force_normal, "\">",
					htmlEscape(rpath->contest->name), "</a>"
				"<div>");
			// Round
			append("<div>"
				"<a href=\"/c/", rpath->round->id, force_normal, "\">",
					htmlEscape(rpath->round->name),
					round_duration(rpath->round->begins, rpath->round->ends),
				"</a>"
			// Problem
				"<a class=\"grayed", status_classes, "\" href=\"/c/",
					rpath->problem->id, force_normal);

			if (link_to_problem_statement)
				append("/statement");

			append("\">", htmlEscape(rpath->problem->name), "</a>"
					"</div>"
				"</div>"
				"</div>");
		}

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
	}
}

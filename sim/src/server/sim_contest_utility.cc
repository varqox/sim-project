#include "sim_contest_utility.h"
#include "sim_session.h"

#include "../simlib/include/debug.h"
#include "../simlib/include/logger.h"
#include "../simlib/include/time.h"

#include <cppconn/prepared_statement.h>
#include <vector>

using std::string;
using std::vector;

string Sim::Contest::submissionStatus(const string& status) {
	if (status == "ok")
		return "Initial tests: OK";

	if (status == "error")
		return "Initial tests: Error";

	if (status == "c_error")
		return "Compilation failed";

	if (status == "judge_error")
		return "Judge error";

	if (status == "waiting")
		return "Pending";

	return "Unknown";
}

string Sim::Contest::convertStringBack(const string& str) {
	string res;
	for (auto i = str.begin(); i != str.end(); ++i) {
		if (*i == '\\' && ++i != str.end() && *i == 'n') {
			res += '\n';
			continue;
		}

		res += *i;
	}
	return res;
}

Sim::Contest::RoundPath* Sim::Contest::getRoundPath(const string& round_id) {
	UniquePtr<RoundPath> r_path(new RoundPath(round_id));

	try {
		struct Local {
			static void copyData(UniquePtr<Round>& r,
					UniquePtr<sql::ResultSet>& res) {
				r.reset(new Round);
				r->id = res->getString(1);
				r->parent = res->getString(2);
				r->problem_id = res->getString(3);
				r->access = res->getString(4);
				r->name = res->getString(5);
				r->owner = res->getString(6);
				r->visible = res->getBoolean(7);
				r->show_ranking = res->getBoolean(8);
				r->begins = res->getString(9);
				r->ends = res->getString(10);
				r->full_results = res->getString(11);
			}
		};

		UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn->
			prepareStatement("SELECT id, parent, problem_id, access, name, "
					"owner, visible, show_ranking, begins, ends, full_results "
				"FROM rounds "
				"WHERE id=? OR id=(SELECT parent FROM rounds WHERE id=?) OR "
					"id=(SELECT grandparent FROM rounds WHERE id=?)"));
		pstmt->setString(1, round_id);
		pstmt->setString(2, round_id);
		pstmt->setString(3, round_id);

		UniquePtr<sql::ResultSet> res(pstmt->executeQuery());

		int rows = res->rowsCount();
		// If round does not exist
		if (rows == 0) {
			sim_.error404();
			return nullptr;
		}

		r_path->type = (rows == 1 ? CONTEST : (rows == 2 ? ROUND : PROBLEM));

		while (res->next()) {
			if (res->isNull(2))
				Local::copyData(r_path->contest, res);
			else if (res->isNull(3)) // problem_id IS NULL
				Local::copyData(r_path->round, res);
			else
				Local::copyData(r_path->problem, res);
		}

		// Check rounds hierarchy
		if (r_path->contest.isNull() || (rows > 1 && r_path->round.isNull())
				|| (rows > 2 && r_path->problem.isNull()))
			throw std::runtime_error("Database error (rounds hierarchy)");

		// Check access
		r_path->admin_access = isAdmin(sim_, *r_path);
		if (!r_path->admin_access) {
			if (r_path->contest->access == "private") {
				// Check access to contest
				if (sim_.session->open() != Sim::Session::OK) {
					sim_.redirect("/login" + sim_.req_->target);
					return nullptr;
				}

				pstmt.reset(sim_.db_conn->
					prepareStatement("SELECT user_id FROM users_to_contests "
						"WHERE user_id=? AND contest_id=?"));
				pstmt->setString(1, sim_.session->user_id);
				pstmt->setString(2, r_path->contest->id);

				res.reset(pstmt->executeQuery());
				if (!res->next()) {
					// User is not assigned to this contest
					sim_.error403();
					return nullptr;
				}
			}

			// Check access to round - check if round has begun
			// If begin time is not null and round has not begun, error403
			if (r_path->type != CONTEST && r_path->round->begins.size() &&
					date("%Y-%m-%d %H:%M:%S") < r_path->round->begins) {
				sim_.error403();
				return nullptr;
			}
		}

	} catch (const std::exception& e) {
		error_log("Caught exception: ", __FILE__, ':', toString(__LINE__),
			" - ", e.what());
		return nullptr;
	}

	return r_path.release();
}

bool Sim::Contest::isAdmin(Sim& sim, const RoundPath& r_path) {
	// If is not logged in, he cannot be admin
	if (sim.session->open() != Sim::Session::OK)
		return false;

	// User is the owner of the contest
	if (r_path.contest->owner == sim.session->user_id)
		return true;

	try {
		// Check if user has more privileges than the owner
		UniquePtr<sql::PreparedStatement> pstmt(sim.db_conn->prepareStatement(
			"SELECT id, type FROM users WHERE id=? OR id=?"));
		pstmt->setString(1, r_path.contest->owner);
		pstmt->setString(2, sim.session->user_id);

		UniquePtr<sql::ResultSet> res(pstmt->executeQuery());
		int owner_type = 0, user_type = 4;
		for (int i = 0; i < 2 && res->next(); ++i) {
			if (res->getString(1) == r_path.contest->owner)
				owner_type = res->getUInt(2);
			else
				user_type = res->getUInt(2);
		}

		return owner_type > user_type;

	} catch (const std::exception& e) {
		error_log("Caught exception: ", __FILE__, ':', toString(__LINE__),
			" - ", e.what());
	}

	return false;
}

Sim::Contest::TemplateWithMenu::TemplateWithMenu(Contest& sim_contest,
	const string& title, const string& styles, const string& scripts)
		: Sim::Template(sim_contest.sim_, title,
			".body{margin-left:190px}" + styles, scripts) {

	*this << "<ul class=\"menu\">\n";

	// Aliases
	string &round_id = sim_contest.r_path_->round_id;
	bool &admin_access = sim_contest.r_path_->admin_access;
	Round *round = sim_contest.r_path_->round.get();

	if (admin_access) {
		*this <<  "<a href=\"/c/add\">Add contest</a>\n"
			"<span>CONTEST ADMINISTRATION</span>\n";

		if (sim_contest.r_path_->type == CONTEST)
			*this << "<a href=\"/c/" << round_id << "/add\">Add round</a>\n"
				"<a href=\"/c/" << round_id << "/edit\">Edit contest</a>\n";

		else if (sim_contest.r_path_->type == ROUND)
			*this << "<a href=\"/c/" << round_id << "/add\">Add problem</a>\n"
				"<a href=\"/c/" << round_id << "/edit\">Edit round</a>\n";

		else // sim_contest.r_path_.type == PROBLEM
			*this << "<a href=\"/c/" << round_id << "/edit\">Edit problem</a>"
				"\n";

		*this << "<a href=\"/c/" << round_id << "\">Dashboard</a>\n"
				"<a href=\"/c/" << round_id << "/problems\">Problems</a>\n"
				"<a href=\"/c/" << round_id << "/submit\">Submit a solution</a>"
					"\n"
				"<a href=\"/c/" << round_id << "/submissions\">Submissions</a>"
					"\n"
				"<a href=\"/c/" << round_id << "/ranking\">Ranking</a>\n"
				"<span>OBSERVER MENU</span>\n";
	}

	*this << "<a href=\"/c/" << round_id << (admin_access ? "/n" : "")
				<< "\">Dashboard</a>\n"
			"<a href=\"/c/" << round_id << (admin_access ? "/n" : "")
				<< "/problems\">Problems</a>\n";

	string current_date = date("%Y-%m-%d %H:%M:%S");
	if (sim_contest.r_path_->type == CONTEST || (
			(round->begins.empty() || round->begins <= current_date) &&
			(round->ends.empty() || current_date < round->ends)))
		*this << "<a href=\"/c/" << round_id << (admin_access ? "/n" : "")
			<< "/submit\">Submit a solution</a>\n";

	*this << "<a href=\"/c/" << round_id << (admin_access ? "/n" : "")
		<< "/submissions\">Submissions</a>\n";

	if (sim_contest.r_path_->contest->show_ranking)
		*this << "<a href=\"/c/" << round_id << (admin_access ? "/n" : "")
			<< "/ranking\">Ranking</a>\n";

	*this << "</ul>";
}

void Sim::Contest::TemplateWithMenu::printRoundPath(const RoundPath& r_path,
		const string& page) {
	*this << "<div class=\"round-path\"><a href=\"/c/" << r_path.contest->id <<
		"/" << page << "\">" << htmlSpecialChars(r_path.contest->name)
		<< "</a>";

	if (r_path.type != CONTEST) {
		*this << " -> <a href=\"/c/" << r_path.round->id << "/" << page << "\">"
			<< htmlSpecialChars(r_path.round->name) << "</a>";

		if (r_path.type == PROBLEM)
			*this << " -> <a href=\"/c/" << r_path.problem->id << "/" << page
				<< "\">"
				<< htmlSpecialChars(r_path.problem->name) << "</a>";
	}

	*this << "</div>\n";
}

namespace {

struct SubroundExtended {
	string id, name, item, begins, ends, full_results;
	bool visible;
};

} // anonymous namespace

void Sim::Contest::TemplateWithMenu::printRoundView(const RoundPath& r_path,
		bool link_to_problem_statement, bool admin_view) {
	try {
		if (r_path.type == CONTEST) {
			// Select subrounds
			UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn->
				prepareStatement(admin_view ?
					"SELECT id, name, item, visible, begins, ends, "
						"full_results FROM rounds WHERE parent=? ORDER BY item"
					: "SELECT id, name, item, visible, begins, ends, "
						"full_results FROM rounds WHERE parent=? AND "
							"(visible=1 OR begins IS NULL OR begins<=?) "
						"ORDER BY item"));
			pstmt->setString(1, r_path.contest->id);
			if (!admin_view)
				pstmt->setString(2, date("%Y-%m-%d %H:%M:%S")); // current date

			UniquePtr<sql::ResultSet> res(pstmt->executeQuery());
			vector<SubroundExtended> subrounds;
			// For performance
			subrounds.reserve(res->rowsCount());

			// Collect results
			while (res->next()) {
				subrounds.emplace_back();
				subrounds.back().id = res->getString(1);
				subrounds.back().name = res->getString(2);
				subrounds.back().item = res->getString(3);
				subrounds.back().visible = res->getBoolean(4);
				subrounds.back().begins = res->getString(5);
				subrounds.back().ends = res->getString(6);
				subrounds.back().full_results = res->getString(7);
			}

			// Select problems
			pstmt.reset(sim_.db_conn->
				prepareStatement("SELECT id, parent, name FROM rounds "
					"WHERE grandparent=? ORDER BY item"));
			pstmt->setString(1, r_path.contest->id);

			res.reset(pstmt->executeQuery());
			std::map<string, vector<Problem> > problems; // (round_id, problems)

			// Fill with all subrounds
			for (size_t i = 0; i < subrounds.size(); ++i)
				problems[subrounds[i].id];

			// Collect results
			while (res->next()) {
				// Get reference to proper vector<Problem>
				__typeof(problems.begin()) it =
					problems.find(res->getString(2));
				// If problem parent is not visible or database error
				if (it == problems.end())
					continue; // Ignore

				vector<Problem>& prob = it->second;
				prob.emplace_back();
				prob.back().id = res->getString(1);
				prob.back().parent = res->getString(2);
				prob.back().name = res->getString(3);
			}

			// Construct "table"
			*this << "<div class=\"round-view\">\n"
				"<a href=\"/c/" << r_path.contest->id << "\"" << ">"
					<< htmlSpecialChars(r_path.contest->name) << "</a>\n"
				"<div>\n";

			// For each subround list all problems
			for (size_t i = 0; i < subrounds.size(); ++i) {
				// Round
				*this << "<div>\n"
					"<a href=\"/c/" << subrounds[i].id << "\">"
					<< htmlSpecialChars(subrounds[i].name) << "</a>\n";

				// List problems
				vector<Problem>& prob = problems[subrounds[i].id];
				for (size_t j = 0; j < prob.size(); ++j) {
					*this << "<a href=\"/c/" << prob[j].id;

					if (link_to_problem_statement)
						*this << "/statement";

					*this << "\">" << htmlSpecialChars(prob[j].name)
						<< "</a>\n";
				}
				*this << "</div>\n";
			}
			*this << "</div>\n"
				"</div>\n";

		} else if (r_path.type == ROUND) {
			// Construct "table"
			*this << "<div class=\"round-view\">\n"
				"<a href=\"/c/" << r_path.contest->id << "\"" << ">"
					<< htmlSpecialChars(r_path.contest->name) << "</a>\n"
				"<div>\n";
			// Round
			*this << "<div>\n"
				"<a href=\"/c/" << r_path.round->id << "\">"
					<< htmlSpecialChars(r_path.round->name) << "</a>\n";

			// Select problems
			UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn->
				prepareStatement("SELECT id, name FROM rounds WHERE parent=? "
					"ORDER BY item"));
			pstmt->setString(1, r_path.round->id);

			// List problems
			UniquePtr<sql::ResultSet> res(pstmt->executeQuery());
			while (res->next()) {
				*this << "<a href=\"/c/" << res->getString(1);

				if (link_to_problem_statement)
					*this << "/statement";

				*this << "\">" << htmlSpecialChars(StringView(
						res->getString(2))) << "</a>\n";
			}
			*this << "</div>\n"
				"</div>\n"
				"</div>\n";

		} else { // r_path.type == PROBLEM
			// Construct "table"
			*this << "<div class=\"round-view\">\n"
				"<a href=\"/c/" << r_path.contest->id << "\"" << ">"
					<< htmlSpecialChars(r_path.contest->name) << "</a>\n"
				"<div>\n";
			// Round
			*this << "<div>\n"
				"<a href=\"/c/" << r_path.round->id << "\">"
					<< htmlSpecialChars(r_path.round->name) << "</a>\n"
				"<a href=\"/c/" << r_path.problem->id;

			if (link_to_problem_statement)
				*this << "/statement";

			*this << "\">" << htmlSpecialChars(r_path.problem->name) << "</a>\n"
					"</div>\n"
				"</div>\n"
				"</div>\n";
		}

	} catch (const std::exception& e) {
		error_log("Caught exception: ", __FILE__, ':', toString(__LINE__),
			" - ", e.what());
	}
}

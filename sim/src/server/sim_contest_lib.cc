#include "sim_contest_lib.h"
#include "sim_session.h"

#include "../include/debug.h"
#include "../include/filesystem.h"
#include "../include/memory.h"
#include "../include/time.h"

#include <cppconn/prepared_statement.h>
#include <cerrno>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

using std::string;
using std::vector;

RoundPath* Contest::getRoundPath(SIM& sim, const string& round_id) {
	RoundPath* rp = new RoundPath(round_id);

	try {
		struct Local {
			static void copyData(Round*& r, UniquePtr<sql::ResultSet>& res) {
				r = new Round;
				r->id = res->getString(1);
				r->parent = res->getString(2);
				r->problem_id = res->getString(3);
				r->access = res->getString(4);
				r->name = res->getString(5);
				r->owner = res->getString(6);
				r->visible = res->getString(7);
				r->begins = res->getString(8);
				r->ends = res->getString(9);
				r->full_results = res->getString(10);
			}

			static void selectRound(Round*& r,
						UniquePtr<sql::PreparedStatement>& pstmt,
						const string& id) {
				pstmt->setString(1, id);
				pstmt->execute();

				// Set results
				UniquePtr<sql::ResultSet> res(pstmt->getResultSet());

				if (res->next())
					copyData(r, res);

				else
					throw std::exception(); // There is error in database...
			}
		};

		UniquePtr<sql::PreparedStatement> pstmt(sim.db_conn()
				->prepareStatement("SELECT id, parent, problem_id, access, name, owner, visible, begins, ends, full_results FROM rounds WHERE id=?"));
		pstmt->setString(1, round_id);
		pstmt->execute();

		UniquePtr<sql::ResultSet> res(pstmt->getResultSet());
		// Round doesn't exist
		if (!res->next()) {
			sim.error404();
			delete rp;
			return NULL;
		}

		if (res->isNull(2)) { // parent IS NULL
			rp->type = CONTEST;
			Local::copyData(rp->contest, res);

		} else if (res->isNull(3)) { // problem_id IS NULL
			rp->type = ROUND;
			Local::copyData(rp->round, res);
			Local::selectRound(rp->contest, pstmt, rp->round->parent);

		} else {
			rp->type = PROBLEM;
			Local::copyData(rp->problem, res);
			Local::selectRound(rp->round, pstmt, rp->problem->parent);
			Local::selectRound(rp->contest, pstmt, rp->round->parent);
		}

		// Check access
		rp->admin_access = isAdmin(sim, *rp);
		if (rp->contest->access == "private" && !rp->admin_access) {
			// Check access to contest
			if (sim.session->open() != SIM::Session::OK) {
				sim.redirect("/login" + sim.req_->target);
				delete rp;
				return NULL;
			}

			pstmt.reset(sim.db_conn()
					->prepareStatement("SELECT user_id FROM users_to_rounds WHERE user_id=? AND round_id=?"));
			pstmt->setString(1, sim.session->user_id);
			pstmt->setString(2, rp->contest->id);
			pstmt->execute();

			res.reset(pstmt->getResultSet());
			if (!res->next()) {
				// User is not assigned to this contest
				sim.error403();
				delete rp;
				return NULL;
			}

			// Check access to round - check if began
			// If not null and hasn't began, error403
			if (rp->type != CONTEST && rp->round->begins.size() &&
					date("%Y-%m-%d %H:%M:%S") < rp->round->begins) {
				sim.error403();
				delete rp;
				return NULL;
			}
		}

	} catch(...) {
		E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
		delete rp;
		return NULL;
	}

	return rp;
}

int Contest::getUserRank(const string& type) {
	if (type == "admin")
		return 0;

	if (type == "teacher")
		return 1;

	return 2;
}

int Contest::getUserRank(SIM& sim, const string& id) {
	try {
		UniquePtr<sql::PreparedStatement> pstmt(sim.db_conn()
				->prepareStatement("SELECT type FROM users WHERE id=?"));
		pstmt->setString(1, id);
		pstmt->execute();

		UniquePtr<sql::ResultSet> res(pstmt->getResultSet());
		if (res->next())
			return getUserRank(res->getString(1));

	} catch(...) {
		E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
	}

	return 3;
}

bool Contest::isAdmin(SIM& sim, const RoundPath& rp) {
	// If is logged in
	if (sim.session->open() == SIM::Session::OK) {
		// User is owner of the contest
		if (rp.contest->owner == sim.session->user_id)
			return true;

		try {
			// Check if user has more privileges than owner
			UniquePtr<sql::PreparedStatement> pstmt(sim.db_conn()
					->prepareStatement("SELECT id, type FROM users WHERE id=? OR id=?"));
			pstmt->setString(1, rp.contest->owner);
			pstmt->setString(2, sim.session->user_id);
			pstmt->execute();

			UniquePtr<sql::ResultSet> res(pstmt->getResultSet());
			if (res->rowsCount() == 2) {
				int owner_type = 0, user_type = 4;

				for (int i = 0; i < 2; ++i) {
					res->next();

					if (res->getString(1) == rp.contest->owner)
						owner_type = getUserRank(res->getString(2));
					else
						user_type = getUserRank(res->getString(2));
				}

				return owner_type > user_type;
			}

		} catch(...) {
			E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
		}
	}

	return false;
}

Contest::TemplateWithMenu::TemplateWithMenu(SIM& sim, const string& title,
		const RoundPath& rp, const string& styles, const string& scripts)
		: SIM::Template(sim, title, ".body{margin-left:190px}" + styles,
			scripts) {

	*this << "<ul class=\"menu\">\n";

	if (rp.admin_access) {
		*this <<  "<a href=\"/c/add\">Add contest</a>\n"
			"<span>CONTEST ADMINISTRATION</span>\n";

		if (rp.type == CONTEST)
			*this << "<a href=\"/c/" << rp.round_id << "/add\">Add round</a>\n"
				"<a href=\"/c/" << rp.round_id << "/edit\">Edit contest</a>\n";

		else if (rp.type == ROUND)
			*this << "<a href=\"/c/" << rp.round_id << "/add\">Add problem</a>\n"
				"<a href=\"/c/" << rp.round_id << "/edit\">Edit round</a>\n";

		else // rp.type == PROBLEM
			*this << "<a href=\"/c/" << rp.round_id << "/edit\">Edit problem</a>\n";

		*this << "<a href=\"/c/" << rp.round_id << "\">Dashboard</a>\n"
				"<a href=\"/c/" << rp.round_id << "/problems\">Problems</a>\n"
				"<a href=\"/c/" << rp.round_id << "/submissions\">Submissions</a>\n"
				"<span>OBSERVER MENU</span>\n";
	}

	*this << "<a href=\"/c/" << rp.round_id << (rp.admin_access ? "/n" : "")
				<< "\">Dashboard</a>\n"
			"<a href=\"/c/" << rp.round_id << (rp.admin_access ? "/n" : "")
				<< "/problems\">Problems</a>\n"
			"<a href=\"/c/" << rp.round_id << (rp.admin_access ? "/n" : "")
				<< "/submissions\">Submissions</a>\n"
		"</ul>";
}

void Contest::printRoundPath(SIM::Template& templ, const RoundPath& rp,
		const string& page) {

	templ << "<div class=\"round-path\"><a href=\"/c/" << rp.contest->id << "/"
		<< page << "\">" << htmlSpecialChars(rp.contest->name) << "</a>";

	if (rp.type != CONTEST) {
		templ << " -> <a href=\"/c/" << rp.round->id << "/" << page << "\">"
			<< htmlSpecialChars(rp.round->name) << "</a>";

		if (rp.type == PROBLEM)
			templ << " -> <a href=\"/c/" << rp.problem->id << "/" << page
				<< "\">"
				<< htmlSpecialChars(rp.problem->name) << "</a>";
	}

	templ << "</div>\n";
}

namespace {

struct Subround {
	string id, name, item, visible, begins, ends, full_results;
};

struct Problem {
	string id, parent, name;
};

} // anonymous namespace

void Contest::printRoundView(SIM& sim, SIM::Template& templ,
		const RoundPath& rp, bool link_to_problem_statement, bool admin_view) {

	try {
		if (rp.type == CONTEST) {
			string curent_date = date("%Y-%m-%d %H:%M:%S");
			// Select subrounds
			UniquePtr<sql::PreparedStatement> pstmt(sim.db_conn()
					->prepareStatement(admin_view ?
						"SELECT id, name, item, visible, begins, ends, full_results FROM rounds WHERE parent=? ORDER BY item"
						: "SELECT id, name, item, visible, begins, ends, full_results FROM rounds WHERE parent=? AND (visible=1 OR begins IS NULL OR begins<=?) ORDER BY item"));
			pstmt->setString(1, rp.contest->id);
			if (!admin_view)
				pstmt->setString(2, curent_date);
			pstmt->execute();

			UniquePtr<sql::ResultSet> res(pstmt->getResultSet());
			vector<Subround> subrounds;
			// For performance
			subrounds.reserve(res->rowsCount());

			// Collect results
			while (res->next()) {
				subrounds.push_back(Subround());
				subrounds.back().id = res->getString(1);
				subrounds.back().name = res->getString(2);
				subrounds.back().item = res->getString(3);
				subrounds.back().visible = res->getString(4);
				subrounds.back().begins = res->getString(5);
				subrounds.back().ends = res->getString(6);
				subrounds.back().full_results = res->getString(7);
			}

			// Select problems
			pstmt.reset(sim.db_conn()
					->prepareStatement("SELECT id, parent, name FROM rounds WHERE grandparent=? ORDER BY item"));
			pstmt->setString(1, rp.contest->id);
			pstmt->execute();

			res.reset(pstmt->getResultSet());
			std::map<string, vector<Problem> > problems; // (round_id, problems)

			// Fill with all subrounds
			for (size_t i = 0; i < subrounds.size(); ++i)
				problems[subrounds[i].id];

			// Colect results
			while (res->next()) {
				// Get reference to proper vector<Problem>
				__typeof(problems.begin()) it =
						problems.find(res->getString(2));
				if (it == problems.end()) // Problem parent isn't visible or database error
					continue; // Ignore

				vector<Problem>& prob = it->second;
				prob.push_back(Problem());
				prob.back().id = res->getString(1);
				prob.back().parent = res->getString(2);
				prob.back().name = res->getString(3);
			}

			// Construct "table"
			templ << "<div class=\"round-view\">\n"
				"<a href=\"/c/" << rp.contest->id << "\"" << ">"
					<< htmlSpecialChars(rp.contest->name) << "</a>\n"
				"<div>\n";

			// For each subround list all problems
			for (size_t i = 0; i < subrounds.size(); ++i) {
				// Round
				templ << "<div>\n"
					"<a href=\"/c/" << subrounds[i].id << "\">"
					<< htmlSpecialChars(subrounds[i].name) << "</a>\n";

				// List problems
				vector<Problem>& prob = problems[subrounds[i].id];
				for (size_t j = 0; j < prob.size(); ++j) {
					templ << "<a href=\"/c/" << prob[j].id;

					if (link_to_problem_statement)
						templ << "/statement";

					templ << "\">" << htmlSpecialChars(prob[j].name)
						<< "</a>\n";
				}
				templ << "</div>\n";
			}
			templ << "</div>\n"
				"</div>\n";

		} else if (rp.type == ROUND) {
			// Construct "table"
			templ << "<div class=\"round-view\">\n"
				"<a href=\"/c/" << rp.contest->id << "\"" << ">"
					<< htmlSpecialChars(rp.contest->name) << "</a>\n"
				"<div>\n";
			// Round
			templ << "<div>\n"
				"<a href=\"/c/" << rp.round->id << "\">"
					<< htmlSpecialChars(rp.round->name) << "</a>\n";

			// Select problems
			UniquePtr<sql::PreparedStatement> pstmt(sim.db_conn()
					->prepareStatement("SELECT id, name FROM rounds WHERE parent=? ORDER BY item"));
			pstmt->setString(1, rp.round->id);
			pstmt->execute();

			// List problems
			UniquePtr<sql::ResultSet> res(pstmt->getResultSet());
			while (res->next()) {
				templ << "<a href=\"/c/" << res->getString(1);

				if (link_to_problem_statement)
					templ << "/statement";

				templ << "\">" << htmlSpecialChars(res->getString(2))
					<< "</a>\n";
			}
			templ << "</div>\n"
				"</div>\n"
				"</div>\n";

		} else { // rp.type == PROBLEM
			// Construct "table"
			templ << "<div class=\"round-view\">\n"
				"<a href=\"/c/" << rp.contest->id << "\"" << ">"
					<< htmlSpecialChars(rp.contest->name) << "</a>\n"
				"<div>\n";
			// Round
			templ << "<div>\n"
				"<a href=\"/c/" << rp.round->id << "\">"
					<< htmlSpecialChars(rp.round->name) << "</a>\n"
				"<a href=\"/c/" << rp.problem->id;

			if (link_to_problem_statement)
				templ << "/statement";

			templ << "\">" << htmlSpecialChars(rp.problem->name) << "</a>\n"
					"</div>\n"
				"</div>\n"
				"</div>\n";
		}

	} catch(...) {
		E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
	}
}

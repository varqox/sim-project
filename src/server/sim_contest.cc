#include "sim.h"
#include "sim_template.h"
#include "sim_session.h"
#include "form_validator.h"

#include "../include/debug.h"
#include "../include/memory.h"
#include "../include/time.h"

#include "cppconn/prepared_statement.h"
#include <vector>

using std::string;
using std::vector;

struct Round {
	string id, parent, problem_id, access, name, owner, visible, begins, ends, full_results;
};

struct StringOrNull {
	bool is_null;
	string str;

	StringOrNull(bool nulled = true) : is_null(nulled), str() {}

	void setNull() { is_null = true; }

	void setString(string s) {
		is_null = false;
		s.swap(str);
	}
};

enum RoundType { CONTEST, ROUND, PROBLEM };

class RoundPath {
private:
	RoundPath(const RoundPath&);
	RoundPath& operator=(const RoundPath&);

public:
	bool admin_access;
	RoundType type;
	string round_id;
	Round *contest, *round, *problem;

	RoundPath(const string& rid): admin_access(false), type(CONTEST),
			round_id(rid), contest(NULL), round(NULL), problem(NULL) {}

	~RoundPath() {
		if (contest)
			delete contest;
		if (round)
			delete round;
		if (problem)
			delete problem;
	}
};

class Contest {
private:
	class TemplateWithMenu : public SIM::Template {
	public:
		TemplateWithMenu(SIM& sim, const string& title, const string& round_id,
				bool admin_view = false, const string& styles = "",
				const string& scripts = "");
	};

	// Pages
	static void addContest(SIM& sim);

	static void addRound(SIM& sim, const RoundPath& rp);

	static void editRound(SIM& sim, const RoundPath& rp);

	static void problems(SIM& sim, const RoundPath& rp,
			bool admin_view = true);

	static void submissions(SIM& sim, const RoundPath& rp,
			bool admin_view = true);

	// Functions
	static RoundPath* getRoundPath(SIM& sim, const string& round_id);

	// Converts string type to int rank: admin -> 0, teacher-> 1, normal -> 2
	static int getUserRank(const string& type);

	static int getUserRank(SIM& sim, const string& id);

	// Returns whether user has admin access
	static bool isAdmin(SIM& sim, const RoundPath& rp);

	static void printRoundPath(SIM::Template& templ, const RoundPath& rp,
		const string& page);

	static void printRoundView(SIM& sim, SIM::Template& templ,
			const RoundPath& rp, bool link_to_problem_statement,
			bool admin_view = false);

	friend class SIM;
};

void SIM::contest() {
	size_t arg_beg = 3;

	if (0 == compareTo(req_->target, arg_beg, '/', "add"))
		return Contest::addContest(*this);

	if (0 == compareTo(req_->target, arg_beg, '/', "")) {
		Template templ(*this, "Select contest");
		try {
			// Get available contests
			UniquePtr<sql::PreparedStatement> pstmt(db_conn()
					->prepareStatement(session->open() != Session::OK ?
						("SELECT id, name FROM rounds WHERE parent IS NULL AND access='public' ORDER BY id")
						: ("(SELECT id, name FROM rounds WHERE parent IS NULL AND (access='public' OR owner=?))"
						" UNION "
						"(SELECT id, name FROM rounds, users_to_rounds WHERE user_id=? AND round_id=id)"
						"ORDER BY id")));
			if (session->open() == Session::OK) {
				pstmt->setString(1, session->user_id);
				pstmt->setString(2, session->user_id);
			}
			pstmt->execute();

			// List them
			UniquePtr<sql::ResultSet> res(pstmt->getResultSet());
			templ << "<div class=\"contests\"><ul>\n";
			while (res->next())
				templ << "<li><a href=\"/c/" << htmlSpecialChars(res->getString(1)) << "\">"
					<< htmlSpecialChars(res->getString(2)) << "</a></li>\n";
			templ << "</ul></div>\n";
		} catch (...) {
			E("\e[31mCaught exception: %s:%d\e[0m\n", __FILE__, __LINE__);
		}
	} else {
		// Extract round id
		string round_id;
		{
			int res_code = strtonum(round_id, req_->target, arg_beg,
					find(req_->target, '/', arg_beg));
			if (res_code == -1)
				return error404();
			arg_beg += res_code + 1;
		}
		// Get parent rounds
		UniquePtr<RoundPath> path(Contest::getRoundPath(*this, round_id));
		if (path.isNull())
			return;
		// Check if user forces observer view
		bool admin_view = path->admin_access;

		if (0 == compareTo(req_->target, arg_beg, '/', "n")) {
			admin_view = false;
			arg_beg += 2;
		}
		// Problem statement
		if (path->type == PROBLEM && 0 == compareTo(req_->target, arg_beg, '/', "statement")) {
			resp_.headers["Content-type"] = "application/pdf";
			resp_.content_type = server::HttpResponse::FILE;
			resp_.content.clear();
			resp_.content.append("problems/").append(path->problem->problem_id).append("/statement.pdf");
			return;
		}
		// Add
		if (0 == compareTo(req_->target, arg_beg, '/', "add"))
			return Contest::addRound(*this, *path);
		// Edit
		if (0 == compareTo(req_->target, arg_beg, '/', "edit"))
			return Contest::editRound(*this, *path);
		// Problems
		if (0 == compareTo(req_->target, arg_beg, '/', "problems"))
			return Contest::problems(*this, *path, admin_view);
		// Submissions
		if (0 == compareTo(req_->target, arg_beg, '/', "submissions"))
			return Contest::submissions(*this, *path, admin_view);

		Contest::TemplateWithMenu templ(*this, "Contest dashboard", round_id, path->admin_access);

		Contest::printRoundPath(templ, *path, "");
		Contest::printRoundView(*this, templ, *path, false, admin_view);
	}
}

void Contest::addContest(SIM& sim) {
	if (sim.session->open() != SIM::Session::OK ||
			getUserRank(sim, sim.session->user_id) > 1)
		return sim.error403();

	FormValidator fv(sim.req_->form_data);
	string name;
	bool is_public = false;

	if (sim.req_->method == server::HttpRequest::POST) {
		// Validate all fields
		fv.validateNotBlank(name, "name", "Contest name", 128);

		is_public = fv.exist("public");

		// If all fields are ok
		if (fv.noErrors())
			try {
				UniquePtr<sql::PreparedStatement> pstmt(sim.db_conn()
						->prepareStatement("INSERT INTO rounds (access, name, owner, item) "
							"SELECT ?, ?, ?, MAX(item)+1 FROM rounds WHERE parent IS NULL"));
				pstmt->setString(1, is_public ? "public" : "private");
				pstmt->setString(2, name);
				pstmt->setString(3, sim.session->user_id);
				if (pstmt->executeUpdate() == 1) {
					UniquePtr<sql::Statement> stmt(sim.db_conn()->createStatement());
					UniquePtr<sql::ResultSet> res(stmt->executeQuery("SELECT LAST_INSERT_ID()"));
					if (res->next())
						sim.redirect("/c/" + res->getString(1));
				}
			} catch (...) {
				E("\e[31mCaught exception: %s:%d\e[0m\n", __FILE__, __LINE__);
			}
	}

	SIM::Template templ(sim, "Add contest", ".body{margin-left:30px}");
	templ << fv.errors() << "<div class=\"form-container\">\n"
			"<h1>Add contest</h1>\n"
			"<form method=\"post\">\n"
				// Name
				"<div class=\"field-group\">\n"
					"<label>Contest name</label>\n"
					"<input type=\"text\" name=\"name\" value=\""
						<< htmlSpecialChars(name) << "\" size=\"24\" maxlength=\"128\">\n"
				"</div>\n"
				// Public
				"<div class=\"field-group\">\n"
					"<label>Public</label>\n"
					"<input type=\"checkbox\" name=\"public\""
						<< (is_public ? " checked" : "") << ">\n"
				"</div>\n"
				"<input type=\"submit\" value=\"Save\">\n"
			"</form>\n"
		"</div>\n";
}

void Contest::addRound(SIM& sim, const RoundPath& rp) {
	if (!rp.admin_access)
		return sim.error403();

	FormValidator fv(sim.req_->form_data);
	string name;
	bool is_visible = false;
	StringOrNull begins, full_results, ends;

	if (sim.req_->method == server::HttpRequest::POST) {
		// Validate all fields
		fv.validateNotBlank(name, "name", "Round name", 128);

		is_visible = fv.exist("visible");

		if (rp.type == CONTEST) {
			begins.is_null = fv.exist("begins_null");
			ends.is_null = fv.exist("ends_null");
			full_results.is_null = fv.exist("full_results_null");

			if (!begins.is_null)
				fv.validateNotBlank(begins.str, "begins", "Ends", is_datetime, "Begins: invalid value");
			if (!ends.is_null)
				fv.validateNotBlank(ends.str, "ends", "Ends", is_datetime, "Ends: invalid value");
			if (!full_results.is_null)
				fv.validateNotBlank(full_results.str, "full_results", "Ends", is_datetime, "Full_results: invalid value");
		}

		// If all fields are ok
		if (fv.noErrors())
			try {
				UniquePtr<sql::PreparedStatement> pstmt;

				if (rp.type == CONTEST) {
					pstmt.reset(sim.db_conn()->prepareStatement(
							"INSERT INTO rounds (parent, name, owner, item, visible, begins, ends, full_results) "
							"SELECT ?, ?, ?, MAX(item)+1, ?, ?, ?, ? FROM rounds WHERE parent=?"));
					pstmt->setString(1, rp.round_id);
					pstmt->setString(2, name);
					pstmt->setString(3, sim.session->user_id);
					pstmt->setBoolean(4, is_visible);
					// Begins
					if (begins.is_null)
						pstmt->setNull(5, 0);
					else
						pstmt->setString(5, begins.str);
					// ends
					if (ends.is_null)
						pstmt->setNull(6, 0);
					else
						pstmt->setString(6, ends.str);
					// Full_results
					if (full_results.is_null)
						pstmt->setNull(7, 0);
					else
						pstmt->setString(7, full_results.str);

					pstmt->setString(8, rp.round_id);
				}

				if (pstmt->executeUpdate() == 1) {
					UniquePtr<sql::Statement> stmt(sim.db_conn()->createStatement());
					UniquePtr<sql::ResultSet> res(stmt->executeQuery("SELECT LAST_INSERT_ID()"));
					if (res->next())
						sim.redirect("/c/" + res->getString(1));
				}
			} catch (...) {
				E("\e[31mCaught exception: %s:%d\e[0m\n", __FILE__, __LINE__);
			}
	}

	TemplateWithMenu templ(sim, "Add round", rp.round_id, rp.admin_access);
	printRoundPath(templ, rp, "");
	if (rp.type == CONTEST) {
		templ << fv.errors() << "<div class=\"form-container\">\n"
			"<h1>Add round</h1>\n"
			"<form method=\"post\">\n"
				// Name
				"<div class=\"field-group\">\n"
					"<label>Round name</label>\n"
					"<input type=\"text\" name=\"name\" value=\""
						<< htmlSpecialChars(name) << "\" size=\"24\" maxlength=\"128\">\n"
				"</div>\n"
				// Visible
				"<div class=\"field-group\">\n"
					"<label>Visible</label>\n"
					"<input type=\"checkbox\" name=\"visible\""
						<< (is_visible ? " checked" : "") << ">\n"
				"</div>\n"
				// Begins
				"<div class=\"field-group\">\n"
					"<label>Begins</label>\n"
					"<input type=\"text\" name=\"begins\""
						"placeholder=\"yyyy-mm-dd HH:MM:SS\" value=\""
						<< htmlSpecialChars(begins.str) << "\" size=\"19\" maxlength=\"19\">\n"
					"<label>Null: </label>\n"
					"<input type=\"checkbox\" name=\"begins_null\""
						<< (begins.is_null ? " checked" : "") << ">\n"
				"</div>\n"
				// Ends
				"<div class=\"field-group\">\n"
					"<label>Ends</label>\n"
					"<input type=\"text\" name=\"ends\""
						"placeholder=\"yyyy-mm-dd HH:MM:SS\" value=\""
						<< htmlSpecialChars(ends.str) << "\" size=\"19\" maxlength=\"19\">\n"
					"<label>Null: </label>\n"
					"<input type=\"checkbox\" name=\"ends_null\""
						<< (ends.is_null ? " checked" : "") << ">\n"
				"</div>\n"
				// Full_results
				"<div class=\"field-group\">\n"
					"<label>Full_results</label>\n"
					"<input type=\"text\" name=\"full_results\""
						"placeholder=\"yyyy-mm-dd HH:MM:SS\" value=\""
						<< htmlSpecialChars(full_results.str) << "\" size=\"19\" maxlength=\"19\">\n"
					"<label>Null: </label>\n"
					"<input type=\"checkbox\" name=\"full_results_null\""
						<< (full_results.is_null ? " checked" : "") << ">\n"
				"</div>\n"
				"<input type=\"submit\" value=\"Save\">\n"
			"</form>\n"
		"</div>\n";
	}
}

void Contest::editRound(SIM& sim, const RoundPath& rp) {
	if (!rp.admin_access)
		return sim.error403();

	TemplateWithMenu templ(sim, "Edit round", rp.round_id, rp.admin_access);
	printRoundPath(templ, rp, "edit");
	templ << "<h1>Edit round</h1>\n";
}

void Contest::problems(SIM& sim, const RoundPath& rp, bool admin_view) {
	TemplateWithMenu templ(sim, "Problems", rp.round_id, rp.admin_access);
	printRoundPath(templ, rp, "problems");
	printRoundView(sim, templ, rp, true, admin_view);
}

void Contest::submissions(SIM& sim, const RoundPath& rp, bool admin_view) {
	TemplateWithMenu templ(sim, "Submissions", rp.round_id, rp.admin_access);
}

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
					throw std::exception(); // It means that there is error in database
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
			if (rp->type != CONTEST && rp->round->begins.size() && date("%Y-%m-%d %H:%M:%S") < rp->round->begins) {
				sim.error403();
				delete rp;
				return NULL;
			}
		}
	} catch(...) {
		E("\e[31mCaught exception: %s:%d\e[0m\n", __FILE__, __LINE__);
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
		E("\e[31mCaught exception: %s:%d\e[0m\n", __FILE__, __LINE__);
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
			E("\e[31mCaught exception: %s:%d\e[0m\n", __FILE__, __LINE__);
		}
	}
	return false;
}

Contest::TemplateWithMenu::TemplateWithMenu(SIM& sim, const string& title,
		const string& round_id, bool admin_view, const string& styles,
		const string& scripts)
		: SIM::Template(sim, title, ".body{margin-left:190px}" + styles,
			scripts) {
	*this << "<ul class=\"menu\">\n";

	if (admin_view)
		*this <<  "<li><a href=\"/c/add\">Add contest</a></li>\n"
			"<span class=\"separator\">CONTEST ADMINISTRATION</span>\n"
			"<li><a href=\"/c/" << round_id << "/add\">Add round</a></li>\n"
			"<li><a href=\"/c/" << round_id << "/edit\">Edit round</a></li>\n"
			"<li><a href=\"/c/" << round_id << "\">Dashboard</a></li>\n"
			"<li><a href=\"/c/" << round_id << "/problems\">Problems</a></li>\n"
			"<li><a href=\"/c/" << round_id << "/submissions\">Submissions</a></li>\n"
			"<span class=\"separator\">OBSERVER MENU</span>\n";

	*this << "<li><a href=\"/c/" << round_id << (admin_view ? "/n" : "")
				<< "\">Dashboard</a></li>\n"
			"<li><a href=\"/c/" << round_id << (admin_view ? "/n" : "")
				<< "/problems\">Problems</a></li>\n"
			"<li><a href=\"/c/" << round_id << (admin_view ? "/n" : "")
				<< "/submissions\">Submissions</a></li>\n"
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

struct Subround {
	string id, name, item, visible, begins, ends, full_results;
};

struct Problem {
	string id, parent, name;
};

/*struct Comparator {
	// Compare string like numbers
	bool operator()(const string& a, const string& b) const {
		return a.size() == b.size() ? a < b : a.size() < b.size();
	}
};*/

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
		E("\e[31mCaught exception: %s:%d\e[0m\n", __FILE__, __LINE__);
	}
}

#include "form_validator.h"
#include "sim.h"
#include "sim_contest_lib.h"
#include "sim_session.h"
#include "sim_template.h"

#include "../include/debug.h"
#include "../include/filesystem.h"
#include "../include/memory.h"
#include "../include/process.h"
#include "../include/time.h"
#include "../include/utility.h"

#include <cppconn/prepared_statement.h>
#include <vector>

using std::string;
using std::vector;

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
			E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
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
		if (path->type == PROBLEM &&
				0 == compareTo(req_->target, arg_beg, '/', "statement")) {
			resp_.headers["Content-type"] = "application/pdf";
			resp_.content_type = server::HttpResponse::FILE;
			resp_.content.clear();
			resp_.content.append("problems/").append(path->problem->problem_id).
				append("/doc/").append(getFileByLines("problems/" +
					path->problem->problem_id + "/conf.cfg" ,
					GFBL_IGNORE_NEW_LINES, 2, 3)[0]);
			return;
		}

		// Add
		if (0 == compareTo(req_->target, arg_beg, '/', "add")) {
			if (path->type == CONTEST)
				return Contest::addRound(*this, *path);

			if (path->type == ROUND)
				return Contest::addProblem(*this, *path);

			return error404();
		}

		// Edit
		if (0 == compareTo(req_->target, arg_beg, '/', "edit")) {
			if (path->type == CONTEST)
				return Contest::editContest(*this, *path);

			if (path->type == ROUND)
				return Contest::editRound(*this, *path);

			return Contest::editProblem(*this, *path);
		}

		// Problems
		if (0 == compareTo(req_->target, arg_beg, '/', "problems"))
			return Contest::problems(*this, *path, admin_view);

		// Submissions
		if (0 == compareTo(req_->target, arg_beg, '/', "submissions"))
			return Contest::submissions(*this, *path, admin_view);

		Contest::TemplateWithMenu templ(*this, "Contest dashboard", *path);
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
				E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
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
				"<input type=\"submit\" value=\"Add\">\n"
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

		begins.is_null = fv.exist("begins_null");
		ends.is_null = fv.exist("ends_null");
		full_results.is_null = fv.exist("full_results_null");

		if (!begins.is_null)
			fv.validateNotBlank(begins.str, "begins", "Ends", is_datetime, "Begins: invalid value");

		if (!ends.is_null)
			fv.validateNotBlank(ends.str, "ends", "Ends", is_datetime, "Ends: invalid value");

		if (!full_results.is_null)
			fv.validateNotBlank(full_results.str, "full_results", "Ends", is_datetime, "Full_results: invalid value");

		// If all fields are ok
		if (fv.noErrors())
			try {
				UniquePtr<sql::PreparedStatement> pstmt(sim.db_conn()->prepareStatement(
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

				if (pstmt->executeUpdate() == 1) {
					UniquePtr<sql::Statement> stmt(sim.db_conn()->createStatement());
					UniquePtr<sql::ResultSet> res(stmt->executeQuery("SELECT LAST_INSERT_ID()"));

					if (res->next())
						sim.redirect("/c/" + res->getString(1));
				}

			} catch (...) {
				E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
			}
	}

	TemplateWithMenu templ(sim, "Add round", rp);
	printRoundPath(templ, rp, "");
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
			"<input type=\"submit\" value=\"Add\">\n"
		"</form>\n"
	"</div>\n";
}

void Contest::addProblem(SIM& sim, const RoundPath& rp) {
	if (!rp.admin_access)
		return sim.error403();

	FormValidator fv(sim.req_->form_data);
	string name, user_package_file;
	bool force_auto_limit = false;

	if (sim.req_->method == server::HttpRequest::POST) {
		// Validate all fields
		fv.validate(name, "name", "Problem name", 128);

		force_auto_limit = fv.exist("force-auto-limit");

		fv.validateNotBlank(user_package_file, "package", "Package");

		// If all fields are OK
		if (fv.noErrors())
			try {
				// Assert that file exists
				string package_file = fv.getFilePath("package");

				if (0 != access(package_file, R_OK))
					return sim.error500();

				// Insert problem
				UniquePtr<sql::PreparedStatement> pstmt(sim.db_conn()->prepareStatement(
						"INSERT INTO problems (owner, added) VALUES(?, ?)"));
				pstmt->setString(1, sim.session->user_id);
				pstmt->setString(2, date("%Y-%m-%d %H:%M:%S"));

				if (1 != pstmt->executeUpdate())
					return sim.error500();

				// Get problem_id
				UniquePtr<sql::Statement> stmt(sim.db_conn()->createStatement());
				UniquePtr<sql::ResultSet> res(stmt->executeQuery("SELECT LAST_INSERT_ID()"));

				if (!res->next())
					return sim.error500();

				string problem_id = res->getString(1);

				// Construct Conver arguments
				vector<string> args(1, "./conver");
				string new_package_file = package_file + '.' +
					(isSuffix(user_package_file, ".tar.gz") ? "tar.gz"
						: getExtension(user_package_file));

				rename(package_file.c_str(), new_package_file.c_str());

				append(args) << new_package_file << "-o"
					<< "problems/" + problem_id << "-uc" << "-q";

				if (force_auto_limit)
					args.push_back("-fal");

				if (name.size())
					append(args) << "-n" << name;

				// Convert package
				if (0 != spawn("./conver", args)) {
					fv.addError("Conver failed");
					// Here should be checker error printing...

					// Remove problem
					pstmt.reset(sim.db_conn()->prepareStatement("DELETE FROM problems WHERE id=?"));
					pstmt->setString(1, problem_id);
					pstmt->executeUpdate();

					remove(new_package_file.c_str());
					goto form;
				}
				remove(new_package_file.c_str());

				// Update problem name
				name = getFileByLines("problems/" + problem_id + "/conf.cfg",
					GFBL_IGNORE_NEW_LINES, 0, 1)[0];
				pstmt.reset(sim.db_conn()->prepareStatement("UPDATE problems SET name=? WHERE id=?"));
				pstmt->setString(1, name);
				pstmt->setString(2, problem_id);
				pstmt->executeUpdate();

				// Insert round
				pstmt.reset(sim.db_conn()->prepareStatement(
					"INSERT INTO rounds (parent, grandparent, name, item, problem_id) "
					"SELECT ?, ?, ?, MAX(item)+1, ? FROM rounds WHERE parent=?"));
				pstmt->setString(1, rp.round->id);
				pstmt->setString(2, rp.contest->id);
				pstmt->setString(3, name);
				pstmt->setString(4, problem_id);
				pstmt->setString(5, rp.round->id);

				if (1 != pstmt->executeUpdate())
					return sim.error500();

				res.reset(stmt->executeQuery("SELECT LAST_INSERT_ID()"));

				if (!res->next())
					return sim.error500();

				sim.redirect("/c/" + res->getString(1));

			} catch (...) {
				E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
			}
	}
 form:

	TemplateWithMenu templ(sim, "Add problem", rp);
	printRoundPath(templ, rp, "");
	templ << fv.errors() << "<div class=\"form-container\">\n"
		"<h1>Add problem</h1>\n"
		"<form method=\"post\" enctype=\"multipart/form-data\">\n"
			// Name
			"<div class=\"field-group\">\n"
				"<label>Problem name</label>\n"
				"<input type=\"text\" name=\"name\" value=\""
					<< htmlSpecialChars(name) << "\" size=\"24\""
				"maxlength=\"128\" placeholder=\"Detect from conf.cfg\">\n"
			"</div>\n"
			// Force auto limit
			"<div class=\"field-group\">\n"
				"<label>Automatic time limit setting</label>\n"
				"<input type=\"checkbox\" name=\"force-auto-limit\""
					<< (force_auto_limit ? " checked" : "") << ">\n"
			"</div>\n"
			// Package
			"<div class=\"field-group\">\n"
				"<label>Package</label>\n"
				"<input type=\"file\" name=\"package\">\n"
			"</div>\n"
			"<input type=\"submit\" value=\"Add\">\n"
		"</form>\n"
	"</div>\n";
}

void Contest::editContest(SIM& sim, const RoundPath& rp) {
	if (!rp.admin_access)
		return sim.error403();

	FormValidator fv(sim.req_->form_data);

	TemplateWithMenu templ(sim, "Edit contest", rp);
}

void Contest::editRound(SIM& sim, const RoundPath& rp) {
	if (!rp.admin_access)
		return sim.error403();

	FormValidator fv(sim.req_->form_data);

	TemplateWithMenu templ(sim, "Edit round", rp);
}

void Contest::editProblem(SIM& sim, const RoundPath& rp) {
	if (!rp.admin_access)
		return sim.error403();

	FormValidator fv(sim.req_->form_data);

	TemplateWithMenu templ(sim, "Edit problem", rp);
}

void Contest::problems(SIM& sim, const RoundPath& rp, bool admin_view) {
	TemplateWithMenu templ(sim, "Problems", rp);
	printRoundPath(templ, rp, "problems");
	printRoundView(sim, templ, rp, true, admin_view);
}

void Contest::submissions(SIM& sim, const RoundPath& rp, bool admin_view) {
	TemplateWithMenu templ(sim, "Submissions", rp);
}

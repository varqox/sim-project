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

#include <cerrno>
#include <cppconn/prepared_statement.h>
#include <utime.h>
#include <vector>

#define foreach(i,x) for (__typeof(x.begin()) i = x.begin(), \
	i ##__end = x.end(); i != i ##__end; ++i)

using std::string;
using std::vector;

void SIM::contest() {
	size_t arg_beg = 3;

	// Select contest
	if (0 == compareTo(req_->target, arg_beg, '/', "")) {
		Template templ(*this, "Select contest");
		try {
			// Get available contests
			UniquePtr<sql::PreparedStatement> pstmt;
			if (session->open() == Session::OK) {
				string lower_owners;
				int rank = Contest::getUserRank(*this, session->user_id);
				if (rank < 2) {
					lower_owners += "OR u.type='normal'";
					if (rank < 1)
						lower_owners += " OR u.type='teacher'";
				}

				pstmt.reset(db_conn()->prepareStatement(
						"(SELECT r.id, r.name FROM rounds r, users u WHERE parent IS NULL AND r.owner=u.id AND "
							"(r.access='public' OR r.owner=? " + lower_owners + "))"
						" UNION "
						"(SELECT id, name FROM rounds, users_to_rounds WHERE user_id=? AND round_id=id)"
						"ORDER BY id"));
				pstmt->setString(1, session->user_id);
				pstmt->setString(2, session->user_id);

			} else
				pstmt.reset(db_conn()->prepareStatement(
					"SELECT id, name FROM rounds WHERE parent IS NULL AND access='public' ORDER BY id"));


			// List them
			UniquePtr<sql::ResultSet> res(pstmt->executeQuery());
			templ << "<div class=\"contests-list\">\n";

			// Add contest button (admins and teachers only)
			if (session->state() == Session::OK &&
					Contest::getUserRank(*this, session->user_id) < 2)
				templ << "<a class=\"btn\" href=\"/c/add\">Add contest</a>\n";

			while (res->next())
				templ << "<a href=\"/c/" << htmlSpecialChars(res->getString(1)) << "\">"
					<< htmlSpecialChars(res->getString(2)) << "</a>\n";
			templ << "</div>\n";

		} catch (const std::exception& e) {
			E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__, __LINE__,
				e.what());

		} catch (...) {
			E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
		}

	// Add contest
	} else if (0 == compareTo(req_->target, arg_beg, '/', "add"))
		Contest::addContest(*this);

	// Other pages which need round id
	else {
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
			string statement = getFileByLines("problems/" +
				path->problem->problem_id + "/conf.cfg", GFBL_IGNORE_NEW_LINES,
				2, 3)[0];

			if (isSuffix(statement, ".pdf"))
				resp_.headers["Content-type"] = "application/pdf";
			else if (isSuffix(statement, ".html") ||
					isSuffix(statement, ".htm"))
				resp_.headers["Content-type"] = "text/html";
			else if (isSuffix(statement, ".txt") || isSuffix(statement, ".md"))
				resp_.headers["Content-type"] = "text/plain; charset=utf-8";

			resp_.content_type = server::HttpResponse::FILE;
			resp_.content.clear();
			resp_.content.append("problems/").append(path->problem->problem_id).
				append("/doc/").append(statement);
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

		// Submit
		if (0 == compareTo(req_->target, arg_beg, '/', "submit"))
			return Contest::submit(*this, *path, admin_view);

		// Submissions
		if (0 == compareTo(req_->target, arg_beg, '/', "submissions"))
			return Contest::submissions(*this, *path, admin_view);

		// Contest dashboard
		Contest::TemplateWithMenu templ(*this, "Contest dashboard", *path);

		templ << "<h1>Dashboard</h1>";
		Contest::printRoundPath(templ, *path, "");
		Contest::printRoundView(*this, templ, *path, false, admin_view);

		if (path->type == PROBLEM)
			templ << "<a class=\"btn\" href=\"/c/" << path->round_id
				<< "/statement\" style=\"margin:5px auto 5px auto\">"
				"View statement</a>\n";
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
						return sim.redirect("/c/" + res->getString(1));
				}

			} catch (const std::exception& e) {
				E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__,
					__LINE__, e.what());

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
						<< htmlSpecialChars(name) << "\" size=\"24\" maxlength=\"128\" required>\n"
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
	string begins, full_results, ends;

	if (sim.req_->method == server::HttpRequest::POST) {
		// Validate all fields
		fv.validateNotBlank(name, "name", "Round name", 128);
		is_visible = fv.exist("visible");
		fv.validate(begins, "begins", "Begins", isDatetimeOrBlank, "Begins: invalid value");
		fv.validate(ends, "ends", "Ends", isDatetimeOrBlank, "Ends: invalid value");
		fv.validate(full_results, "full_results", "Ends", isDatetimeOrBlank, "Full_results: invalid value");

		// If all fields are ok
		if (fv.noErrors())
			try {
				UniquePtr<sql::PreparedStatement> pstmt(sim.db_conn()->prepareStatement(
						"INSERT INTO rounds (parent, name, item, visible, begins, ends, full_results) "
						"SELECT ?, ?, MAX(item)+1, ?, ?, ?, ? FROM rounds WHERE parent=?"));
				pstmt->setString(1, rp.round_id);
				pstmt->setString(2, name);
				pstmt->setBoolean(3, is_visible);

				// Begins
				if (begins.empty())
					pstmt->setNull(4, 0);
				else
					pstmt->setString(4, begins);

				// ends
				if (ends.empty())
					pstmt->setNull(5, 0);
				else
					pstmt->setString(5, ends);

				// Full_results
				if (full_results.empty())
					pstmt->setNull(6, 0);
				else
					pstmt->setString(6, full_results);

				pstmt->setString(7, rp.round_id);

				if (pstmt->executeUpdate() == 1) {
					UniquePtr<sql::Statement> stmt(sim.db_conn()->createStatement());
					UniquePtr<sql::ResultSet> res(stmt->executeQuery("SELECT LAST_INSERT_ID()"));

					if (res->next())
						return sim.redirect("/c/" + res->getString(1));
				}

			} catch (const std::exception& e) {
				E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__,
					__LINE__, e.what());

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
					<< htmlSpecialChars(name) << "\" size=\"24\" maxlength=\"128\" required>\n"
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
					<< htmlSpecialChars(begins) << "\" size=\"19\" maxlength=\"19\">\n"
			"</div>\n"
			// Ends
			"<div class=\"field-group\">\n"
				"<label>Ends</label>\n"
				"<input type=\"text\" name=\"ends\""
					"placeholder=\"yyyy-mm-dd HH:MM:SS\" value=\""
					<< htmlSpecialChars(ends) << "\" size=\"19\" maxlength=\"19\">\n"
			"</div>\n"
			// Full_results
			"<div class=\"field-group\">\n"
				"<label>Full_results</label>\n"
				"<input type=\"text\" name=\"full_results\""
					"placeholder=\"yyyy-mm-dd HH:MM:SS\" value=\""
					<< htmlSpecialChars(full_results) << "\" size=\"19\" maxlength=\"19\">\n"
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

				append(args)(new_package_file)("-o")("problems/" + problem_id)
					("-uc")("-q");

				if (force_auto_limit)
					args.push_back("-fal");

				if (name.size())
					append(args)("-n")(name);

				// Conver stdin, stdout, stderr
				char tmp_filename[] = "/tmp/sim-conver-errors.XXXXXX";
				spawn_opts sopt = {
					-1,
					-1,
					mkstemp(tmp_filename)
				};

				// Convert package
				if (0 != spawn("./conver", args, &sopt)) {
					fv.addError("Conver failed - " + (sopt.new_stderr_fd ?
						getFileContents(tmp_filename) : ""));

					// Remove problem
					try {
						pstmt.reset(sim.db_conn()->prepareStatement("DELETE FROM problems WHERE id=?"));
						pstmt->setString(1, problem_id);
						pstmt->executeUpdate();

					// Suppress all exceptions
					} catch (...) {}

					// Clean
					remove(new_package_file.c_str());

					if (sopt.new_stderr_fd >= 0) {
						unlink(tmp_filename);
						while (close(sopt.new_stderr_fd) == -1 &&
								errno == EINTR) {}
					}

					goto form;
				}

				// Clean
				remove(new_package_file.c_str());

				if (sopt.new_stderr_fd >= 0) {
					unlink(tmp_filename);
					while (close(sopt.new_stderr_fd) == -1 && errno == EINTR) {}
				}

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

				return sim.redirect("/c/" + res->getString(1));

			} catch (const std::exception& e) {
				E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__,
					__LINE__, e.what());

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
					"<input type=\"file\" name=\"package\" required>\n"
				"</div>\n"
				"<input type=\"submit\" value=\"Add\">\n"
			"</form>\n"
		"</div>\n";
}

void Contest::editContest(SIM& sim, const RoundPath& rp) {
	if (!rp.admin_access)
		return sim.error403();

	FormValidator fv(sim.req_->form_data);
	string name, owner;
	bool is_public = false;

	if (sim.req_->method == server::HttpRequest::POST) {
		// Validate all fields
		fv.validateNotBlank(name, "name", "Contest name", 128);
		fv.validateNotBlank(owner, "owner", "Owner username", 30);
		is_public = fv.exist("public");

		// If all fields are ok
		if (fv.noErrors())
			try {
				UniquePtr<sql::PreparedStatement> pstmt(sim.db_conn()
					->prepareStatement("UPDATE rounds r, (SELECT id FROM users WHERE username=?) u SET name=?, owner=u.id, access=? WHERE r.id=?"));
				pstmt->setString(1, owner);
				pstmt->setString(2, name);
				pstmt->setString(3, is_public ? "public" : "private");
				pstmt->setString(4, rp.round_id);

				if (pstmt->executeUpdate() == 1) {
					fv.addError("Update successful");
					// Update rp
					UniquePtr<RoundPath> new_rp(getRoundPath(sim, rp.round_id));
					const_cast<RoundPath&>(rp).swap(*new_rp);
				} else
					fv.addError("Update failed");

			} catch (const std::exception& e) {
				E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__,
					__LINE__, e.what());

			} catch (...) {
				E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
			}
	}

	// Get contest information
	UniquePtr<sql::PreparedStatement> pstmt(sim.db_conn()
		->prepareStatement("SELECT r.name, u.username, r.access FROM rounds r, users u WHERE r.id=? AND r.owner=u.id"));
	pstmt->setString(1, rp.round_id);

	UniquePtr<sql::ResultSet> res(pstmt->executeQuery());
	if (res->next()) {
		name = res->getString(1);
		owner = res->getString(2);
		is_public = res->getString(3) == "public";
	}

	TemplateWithMenu templ(sim, "Edit contest", rp);
	printRoundPath(templ, rp, "");
	templ << fv.errors() << "<div class=\"form-container\">\n"
			"<h1>Edit contest</h1>\n"
			"<form method=\"post\">\n"
				// Name
				"<div class=\"field-group\">\n"
					"<label>Contest name</label>\n"
					"<input type=\"text\" name=\"name\" value=\""
						<< htmlSpecialChars(name) << "\" size=\"24\" maxlength=\"128\" required>\n"
				"</div>\n"
				// Owner
				"<div class=\"field-group\">\n"
					"<label>Owner username</label>\n"
					"<input type=\"text\" name=\"owner\" value=\""
						<< htmlSpecialChars(owner) << "\" size=\"24\" maxlength=\"30\" required>\n"
				"</div>\n"
				// Public
				"<div class=\"field-group\">\n"
					"<label>Public</label>\n"
					"<input type=\"checkbox\" name=\"public\""
						<< (is_public ? " checked" : "") << ">\n"
				"</div>\n"
				"<input type=\"submit\" value=\"Update\">\n"
			"</form>\n"
		"</div>\n";
}

void Contest::editRound(SIM& sim, const RoundPath& rp) {
	if (!rp.admin_access)
		return sim.error403();

	FormValidator fv(sim.req_->form_data);
	string name;
	bool is_visible = false;
	string begins, full_results, ends;

	if (sim.req_->method == server::HttpRequest::POST) {
		// Validate all fields
		fv.validateNotBlank(name, "name", "Round name", 128);
		is_visible = fv.exist("visible");
		fv.validate(begins, "begins", "Begins", isDatetimeOrBlank, "Begins: invalid value");
		fv.validate(ends, "ends", "Ends", isDatetimeOrBlank, "Ends: invalid value");
		fv.validate(full_results, "full_results", "Ends", isDatetimeOrBlank, "Full_results: invalid value");

		// If all fields are ok
		if (fv.noErrors())
			try {
				UniquePtr<sql::PreparedStatement> pstmt(sim.db_conn()->prepareStatement(
						"UPDATE rounds SET name=?, visible=?, begins=?, ends=?, full_results=? WHERE id=?"));
				pstmt->setString(1, name);
				pstmt->setBoolean(2, is_visible);

				// Begins
				if (begins.empty())
					pstmt->setNull(3, 0);
				else
					pstmt->setString(3, begins);

				// ends
				if (ends.empty())
					pstmt->setNull(4, 0);
				else
					pstmt->setString(4, ends);

				// Full_results
				if (full_results.empty())
					pstmt->setNull(5, 0);
				else
					pstmt->setString(5, full_results);

				pstmt->setString(6, rp.round_id);

				if (pstmt->executeUpdate() == 1) {
					fv.addError("Update successful");
					// Update rp
					UniquePtr<RoundPath> new_rp(getRoundPath(sim, rp.round_id));
					const_cast<RoundPath&>(rp).swap(*new_rp);
				} else
					fv.addError("Update failed");

			} catch (const std::exception& e) {
				E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__,
					__LINE__, e.what());

			} catch (...) {
				E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
			}
	}

	// Get round information
	UniquePtr<sql::PreparedStatement> pstmt(sim.db_conn()
		->prepareStatement("SELECT name, visible, begins, ends, full_results FROM rounds WHERE id=?"));
	pstmt->setString(1, rp.round_id);

	UniquePtr<sql::ResultSet> res(pstmt->executeQuery());
	if (res->next()) {
		name = res->getString(1);
		is_visible = res->getBoolean(2);
		begins = res->getString(3);
		ends = res->getString(4);
		full_results = res->getString(5);
	}

	TemplateWithMenu templ(sim, "Edit round", rp);
	printRoundPath(templ, rp, "");
	templ << fv.errors() << "<div class=\"form-container\">\n"
		"<h1>Edit round</h1>\n"
		"<form method=\"post\">\n"
			// Name
			"<div class=\"field-group\">\n"
				"<label>Round name</label>\n"
				"<input type=\"text\" name=\"name\" value=\""
					<< htmlSpecialChars(name) << "\" size=\"24\" maxlength=\"128\" required>\n"
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
					<< htmlSpecialChars(begins) << "\" size=\"19\" maxlength=\"19\">\n"
			"</div>\n"
			// Ends
			"<div class=\"field-group\">\n"
				"<label>Ends</label>\n"
				"<input type=\"text\" name=\"ends\""
					"placeholder=\"yyyy-mm-dd HH:MM:SS\" value=\""
					<< htmlSpecialChars(ends) << "\" size=\"19\" maxlength=\"19\">\n"
			"</div>\n"
			// Full_results
			"<div class=\"field-group\">\n"
				"<label>Full_results</label>\n"
				"<input type=\"text\" name=\"full_results\""
					"placeholder=\"yyyy-mm-dd HH:MM:SS\" value=\""
					<< htmlSpecialChars(full_results) << "\" size=\"19\" maxlength=\"19\">\n"
			"</div>\n"
			"<input type=\"submit\" value=\"Update\">\n"
		"</form>\n"
	"</div>\n";
}

void Contest::editProblem(SIM& sim, const RoundPath& rp) {
	if (!rp.admin_access)
		return sim.error403();

	FormValidator fv(sim.req_->form_data);

	TemplateWithMenu templ(sim, "Edit problem", rp);
}

void Contest::problems(SIM& sim, const RoundPath& rp, bool admin_view) {
	TemplateWithMenu templ(sim, "Problems", rp);
	templ << "<h1>Problems</h1>";
	printRoundPath(templ, rp, "problems");
	printRoundView(sim, templ, rp, true, admin_view);
}

namespace {

struct Subround {
	string id, name;
};

} // anonymous namespace

void Contest::submit(SIM& sim, const RoundPath& rp, bool admin_view) {
	if (sim.session->open() != SIM::Session::OK)
		return sim.redirect("/login" + sim.req_->target);

	FormValidator fv(sim.req_->form_data);

	if (sim.req_->method == server::HttpRequest::POST) {
		string solution, problem_round_id;

		// Validate all fields
		fv.validateNotBlank(solution, "solution", "Solution file field");

		fv.validateNotBlank(problem_round_id, "round-id", "Problem");

		if (!isInteger(problem_round_id))
			fv.addError("Wrong problem round id");

		// If all fields are ok
		if (fv.noErrors()) {
			UniquePtr<RoundPath> path;
			const RoundPath *prp = &rp;

			if (rp.type != PROBLEM) {
				// Get parent rounds of problem round
				path.reset(getRoundPath(sim, problem_round_id));
				if (path.isNull())
					return;

				if (path->type != PROBLEM) {
					fv.addError("Wrong problem round id");
					goto form;
				}

				prp = path.get();
			}

			string solution_tmp_path = fv.getFilePath("solution");
			struct stat sb;
			stat(solution_tmp_path.c_str(), &sb);

			// Check if solution is too big
			if (sb.st_size > 100 << 10) { // 100kB
				fv.addError("Solution file to big (max 100kB)");
				goto form;
			}

			try {
				string current_date = date("%Y-%m-%d %H:%M:%S");
				// Insert submission to `submissions`
				UniquePtr<sql::PreparedStatement> pstmt(sim.db_conn()
					->prepareStatement("INSERT INTO submissions (user_id, problem_id, round_id, parent_round_id, contest_round_id, submit_time, queued) VALUES(?, ?, ?, ?, ?, ?, ?)"));
				pstmt->setString(1, sim.session->user_id);
				pstmt->setString(2, prp->problem->problem_id);
				pstmt->setString(3, prp->problem->id);
				pstmt->setString(4, prp->round->id);
				pstmt->setString(5, prp->contest->id);
				pstmt->setString(6, current_date);
				pstmt->setString(7, current_date);

				if (1 != pstmt->executeUpdate()) {
					fv.addError("Database error - failed to insert submission");
					goto form;
				}

				// Get inserted submission id
				UniquePtr<sql::Statement> stmt(sim.db_conn()->createStatement());
				UniquePtr<sql::ResultSet> res(stmt->executeQuery("SELECT LAST_INSERT_ID()"));

				if (!res->next()) {
					fv.addError("Database error - failed to get inserted submission id");
					goto form;
				}

				string submission_id = res->getString(1);

				// Copy solution
				copy(solution_tmp_path, "solutions/" + submission_id + ".cpp");

				// Change submission status to 'waiting'
				stmt->executeUpdate("UPDATE submissions SET status='waiting' WHERE id=" + submission_id);

				// Insert submission to `submissions_to_rounds`
				pstmt.reset(sim.db_conn()->prepareStatement(
						"INSERT INTO submissions_to_rounds (submission_id, user_id, round_id, submit_time) VALUES(?, ?, ?, ?)"));

				const string arr[] = { prp->problem->id, prp->round->id,
					prp->contest->id };
				for (size_t i = 0; i < sizeof(arr) / sizeof(*arr); ++i) {
					pstmt->setString(1, submission_id);
					pstmt->setString(2, sim.session->user_id);
					pstmt->setString(3, arr[i]);
					pstmt->setString(4, current_date);
					pstmt->executeUpdate();
				}

				// Notify judge-machine
				utime("judge-machine.notify", NULL);

				return sim.redirect("/s/" + submission_id);

			} catch (const std::exception& e) {
				fv.addError("Internal server error");
				E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__,
					__LINE__, e.what());

			} catch (...) {
				fv.addError("Internal server error");
				E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
			}
		}
	}

 form:
	TemplateWithMenu templ(sim, "Submit a solution", rp);
	printRoundPath(templ, rp, "");
	string buffer;
	append(buffer) << fv.errors() << "<div class=\"form-container\">\n"
			"<h1>Submit a solution</h1>\n"
			"<form method=\"post\" enctype=\"multipart/form-data\">\n"
				// Round id
				"<div class=\"field-group\">\n"
					"<label>Problem</label>\n"
					"<select name=\"round-id\">";

	// List problems
	try {
		string current_date = date("%Y-%m-%d %H:%M:%S");
		if (rp.type == CONTEST) {
			// Select subrounds
			// Admin -> All problems from all subrounds
			// Normal -> All problems from subrounds which have begun and
			// have not ended
			UniquePtr<sql::PreparedStatement> pstmt(sim.db_conn()
					->prepareStatement(admin_view ?
						"SELECT id, name FROM rounds WHERE parent=? ORDER BY item"
						: "SELECT id, name FROM rounds WHERE parent=? AND (begins IS NULL OR begins<=?) AND (ends IS NULL OR ?<ends) ORDER BY item"));
			pstmt->setString(1, rp.contest->id);
			if (!admin_view) {
				pstmt->setString(2, current_date);
				pstmt->setString(3, current_date);
			}

			UniquePtr<sql::ResultSet> res(pstmt->executeQuery());
			vector<Subround> subrounds;
			// For performance
			subrounds.reserve(res->rowsCount());

			// Collect results
			while (res->next()) {
				subrounds.push_back(Subround());
				subrounds.back().id = res->getString(1);
				subrounds.back().name = res->getString(2);
			}

			// Select problems
			pstmt.reset(sim.db_conn()
					->prepareStatement("SELECT id, parent, name FROM rounds WHERE grandparent=? ORDER BY item"));
			pstmt->setString(1, rp.contest->id);

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
				prob.push_back(Problem());
				prob.back().id = res->getString(1);
				prob.back().parent = res->getString(2);
				prob.back().name = res->getString(3);
			}

			// For each subround list all problems
			foreach (subround, subrounds) {
				vector<Problem>& prob = problems[subround->id];

				foreach (problem, prob)
					append(buffer) << "<option value=\"" << problem->id << "\">"
						<< htmlSpecialChars(problem->name) << " ("
						<< htmlSpecialChars(subround->name) << ")</option>\n";
			}

		// Admin -> All problems
		// Normal -> if round has begun and has not ended
		} else if (rp.type == ROUND && (admin_view || (
				rp.round->begins <= current_date && // "" <= everything
				(rp.round->ends.empty() || current_date < rp.round->ends)))) {
			// Select problems
			UniquePtr<sql::PreparedStatement> pstmt(sim.db_conn()
					->prepareStatement("SELECT id, name FROM rounds WHERE parent=? ORDER BY item"));
			pstmt->setString(1, rp.round->id);

			// List problems
			UniquePtr<sql::ResultSet> res(pstmt->executeQuery());
			while (res->next())
				append(buffer) << "<option value=\"" << res->getString(1)
					<< "\">" << htmlSpecialChars(res->getString(2)) << " ("
					<< htmlSpecialChars(rp.round->name) << ")</option>\n";

		// Admin -> Current problem
		// Normal -> if parent round has begun and has not ended
		} else if (rp.type == PROBLEM && (admin_view || (
				rp.round->begins <= current_date && // "" <= everything
				(rp.round->ends.empty() || current_date < rp.round->ends)))) {
			append(buffer) << "<option value=\"" << rp.problem->id << "\">"
				<< htmlSpecialChars(rp.problem->name) << " ("
				<< htmlSpecialChars(rp.round->name) << ")</option>\n";
		}

	} catch (const std::exception& e) {
		E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__, __LINE__,
			e.what());

	} catch (...) {
		E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
	}

	if (isSuffix(buffer, "</option>\n"))
		templ << buffer << "</select>"
					"</div>\n"
					// Solution file
					"<div class=\"field-group\">\n"
						"<label>Solution</label>\n"
						"<input type=\"file\" name=\"solution\" required>\n"
					"</div>\n"
					"<input type=\"submit\" value=\"Submit\">\n"
				"</form>\n"
			"</div>\n";

	else
		templ << "<p>There are no problems for which you can submit a solution...</p>";
}

static string submissionStatus(const string& status) {
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

void Contest::submissions(SIM& sim, const RoundPath& rp, bool admin_view) {
	if (sim.session->open() != SIM::Session::OK)
		return sim.redirect("/login" + sim.req_->target);

	TemplateWithMenu templ(sim, "Submissions", rp);
	templ << "<h1>Submissions</h1>";
	printRoundPath(templ, rp, "submissions");

	templ << "<h3>Submission queue size: ";
	try {
		UniquePtr<sql::Statement> stmt(sim.db_conn()->createStatement());
		UniquePtr<sql::ResultSet> res(stmt->executeQuery(
			"SELECT COUNT(*) FROM submissions WHERE status='waiting';"));
		if (res->next())
			templ << res->getString(1);

	} catch (const std::exception& e) {
		E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__, __LINE__,
			e.what());

	} catch (...) {
		E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
	}

	templ <<  "</h3>";

	try {
		UniquePtr<sql::PreparedStatement> pstmt(sim.db_conn()->prepareStatement(
			admin_view ? "SELECT s.id, str.submit_time, r2.id, r2.name, r.id, r.name, s.status, s.score, str.final, u.username "
				"FROM submissions_to_rounds str, submissions s, users u, rounds r, rounds r2 "
				"WHERE str.submission_id=s.id AND s.round_id=r.id AND r.parent=r2.id "
					"AND str.round_id=? AND str.user_id=u.id "
				"ORDER BY str.submit_time DESC"
			: "SELECT s.id, str.submit_time, r3.id, r3.name, r2.id, r2.name, s.status, s.score, str.final, r3.full_results "
				"FROM submissions_to_rounds str, submissions s, rounds r, rounds r2, rounds r3 "
				"WHERE str.submission_id=s.id AND r.id=str.round_id AND s.round_id=r2.id "
					"AND r2.parent=r3.id AND str.round_id=? AND str.user_id=? "
				"ORDER BY str.submit_time DESC"));
		pstmt->setString(1, rp.round_id);
		if (!admin_view)
			pstmt->setString(2, sim.session->user_id);

		UniquePtr<sql::ResultSet> res(pstmt->executeQuery());
		if (res->rowsCount() == 0) {
			templ << "<p>There are no submissions to show</p>";
			return;
		}

		templ << "<table class=\"submissions\">\n"
			"<thead>\n"
				"<tr>"
					<< (admin_view ? "<th class=\"user\">User</th>" : "")
					<< "<th class=\"time\">Submission time</th>"
					"<th class=\"problem\">Problem</th>"
					"<th class=\"status\">Status</th>"
					"<th class=\"score\">Score</th>"
					"<th class=\"final\">Final</th>"
				"</tr>\n"
			"</thead>\n"
			"<tbody>\n";

		struct Local {
			static string statusRow(const string& status) {
				string ret = "<td";

				if (status == "ok")
					ret += " class=\"ok\">";
				else if (status == "error")
					ret += " class=\"wa\">";
				else if (status == "c_error" || status == "judge_error")
					ret += " class=\"tl-rte\">";
				else
					ret += ">";

				return ret.append(submissionStatus(status)).append("</td>");
			}
		};

		string current_date = date("%Y-%m-%d %H:%M:%S");
		while (res->next()) {
			templ << "<tr>";
			// User
			if (admin_view)
				templ << "<td>" << htmlSpecialChars(res->getString(10))
					<< "</td>";
			// Rest
			templ << "<td><a href=\"/s/" << res->getString(1) << "\">"
					<< res->getString(2) << "</a></td>"
					"<td><a href=\"/c/" << res->getString(3) << "\">"
						<< htmlSpecialChars(res->getString(4)) << "</a>"
						" -> <a href=\"/c/" << res->getString(5) << "\">"
						<< htmlSpecialChars(res->getString(6)) << "</a></td>"
					<< Local::statusRow(res->getString(7))
					<< "<td>" << (admin_view ||
						string(res->getString(10)) <= current_date ?
						res->getString(8) : "") << "</td>"
					"<td>" << (res->getBoolean(9) ? "Yes" : "") << "</td>"
				"<tr>\n";
		}

		templ << "</tbody>\n"
			"</table>\n";

	} catch (const std::exception& e) {
		E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__, __LINE__,
			e.what());

	} catch (...) {
		E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
	}

}

static string convertStringBack(const string& str) {
	string res;
	foreach (i, str) {
		if (*i == '\\') {
			if (*++i == 'n') {
				res += '\n';
				continue;
			}

			--i;
		}

		res += *i;
	}

	return res;
}

void SIM::submission() {
	if (session->open() != Session::OK)
		return redirect("/login" + req_->target);

	size_t arg_beg = 3;

	// Extract round id
	string submission_id;
	{
		int res_code = strtonum(submission_id, req_->target, arg_beg,
				find(req_->target, '/', arg_beg));
		if (res_code == -1)
			return error404();

		arg_beg += res_code + 1;
	}

	try {
		// Get submission
		UniquePtr<sql::PreparedStatement> pstmt(db_conn()
			->prepareStatement("SELECT user_id, round_id, submit_time, status, score, name FROM submissions s, problems p WHERE s.id=? AND s.problem_id=p.id"));
		pstmt->setString(1, submission_id);

		UniquePtr<sql::ResultSet> res(pstmt->executeQuery());
		if (!res->next())
			return error404();

		string submission_user_id = res->getString(1);
		string round_id = res->getString(2);
		string submit_time = res->getString(3);
		string submission_status = res->getString(4);
		string score = res->getString(5);
		string problem_name = res->getString(6);

		// Get parent rounds
		UniquePtr<RoundPath> path(Contest::getRoundPath(*this, round_id));
		if (path.isNull())
			return;

		if (!path->admin_access && session->user_id != submission_user_id)
			return error403();

		// Check if user forces observer view
		bool admin_view = path->admin_access;
		if (0 == compareTo(req_->target, arg_beg, '/', "n")) {
			admin_view = false;
			arg_beg += 2;
		}

		// Download solution
		if (0 == compareTo(req_->target, arg_beg, '/', "download")) {
			resp_.headers["Content-type"] = "application/text";
			resp_.headers["Content-Disposition"] = "attchment; filename=" +
				submission_id + ".cpp";

			resp_.content = "solutions/" + submission_id + ".cpp";
			resp_.content_type = server::HttpResponse::FILE;

			return;
		}

		Contest::TemplateWithMenu templ(*this, "Submission " + submission_id,
			*path);
		Contest::printRoundPath(templ, *path, "");

		// View source
		if (0 == compareTo(req_->target, arg_beg, '/', "source")) {
			vector<string> args;
			append(args)("./CTH")("solutions/" + submission_id + ".cpp");

			char tmp_filename[] = "/tmp/sim-server-tmp.XXXXXX";
			spawn_opts sopt = {
				-1,
				mkstemp(tmp_filename),
				-1
			};

			spawn(args[0], args, &sopt);
			if (sopt.new_stdout_fd >= 0)
				templ << getFileContents(tmp_filename);

			unlink(tmp_filename);
			if (sopt.new_stdout_fd >= 0)
				while (close(sopt.new_stderr_fd) == -1 && errno == EINTR) {}

			return;
		}

		templ << "<div class=\"submission-info\">\n"
			"<div>\n"
				"<h1>Submission " << submission_id << "</h1>\n"
				"<div>\n"
					"<a class=\"btn-small\" href=\""
						<< req_->target.substr(0, arg_beg - 1)
						<< "/source\">View source</a>\n"
					"<a class=\"btn-small\" href=\""
						<< req_->target.substr(0, arg_beg - 1)
						<< "/download\">Download</a>\n"
				"</div>\n"
			"</div>\n"
			"<table style=\"width: 100%\">\n"
				"<thead>\n"
					"<tr>"
						"<th style=\"min-width:120px\">Problem</th>"
						"<th style=\"min-width:150px\">Submission time</th>"
						"<th style=\"min-width:150px\">Status</th>"
						"<th style=\"min-width:90px\">Score</th>"
					"</tr>\n"
				"</thead>\n"
				"<tbody>\n"
					"<tr>"
						"<td>" << htmlSpecialChars(problem_name) << "</td>"
						"<td>" << htmlSpecialChars(submit_time) << "</td>"
						"<td";

		if (submission_status == "ok")
			templ << " class=\"ok\"";
		else if (submission_status == "error")
			templ << " class=\"wa\"";
		else if (submission_status == "c_error" ||
				submission_status == "judge_error")
			templ << " class=\"tl-rte\"";

		templ <<			">" << submissionStatus(submission_status)
							<< "</td>"
						"<td>" << (admin_view ||
							path->round->full_results.empty() ||
							path->round->full_results <=
								date("%Y-%m-%d %H:%M:%S") ? score : "")
							<< "</td>"
					"</tr>\n"
				"</tbody>\n"
			"</table>\n"
			<< "</div>\n";

		// Show testing report
		vector<string> submission_file =
			getFileByLines("submissions/" + submission_id,
				GFBL_IGNORE_NEW_LINES);

		templ << "<div class=\"results\">";
		if (submission_status == "c_error" && submission_file.size() > 0) {
			templ << "<h2>Compilation failed</h2>"
				"<pre class=\"compile-errors\">"
				<< convertStringBack(submission_file[0])
				<< "</pre>";

		} else if (submission_file.size() > 1) {
			if (admin_view || path->round->full_results.empty() ||
					path->round->full_results <= date("%Y-%m-%d %H:%M:%S"))
				templ << "<h2>Final testing report</h2>"
					<< convertStringBack(submission_file[0]);

			templ << "<h2>Initial testing report</h2>"
				<< convertStringBack(submission_file[1]);
		}
		templ << "</div>";

	} catch (const std::exception& e) {
		E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__, __LINE__,
			e.what());

	} catch (...) {
		E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
	}
}

#include "form_validator.h"
#include "sim_contest_utility.h"
#include "sim_session.h"

#include "../simlib/include/config_file.h"
#include "../simlib/include/debug.h"
#include "../simlib/include/filesystem.h"
#include "../simlib/include/process.h"
#include "../simlib/include/sim_problem.h"
#include "../simlib/include/time.h"

#include <cerrno>
#include <cppconn/prepared_statement.h>

#define foreach(i,x) for (__typeof(x.begin()) i = x.begin(), \
	i ##__end = x.end(); i != i ##__end; ++i)

using std::string;
using std::vector;

Sim::Contest::~Contest() { delete r_path_; }

void Sim::Contest::handle() {
	arg_beg = 3;

	// Select contest
	if (0 == compareTo(sim_.req_->target, arg_beg, '/', "")) {
		Template templ(sim_, "Select contest");
		try {
			// Get available contests
			UniquePtr<sql::PreparedStatement> pstmt;
			if (sim_.session->open() == Session::OK) {
				string lower_owners;

				pstmt.reset(sim_.db_conn()->prepareStatement(
					"(SELECT r.id, r.name FROM rounds r, users u "
						"WHERE parent IS NULL AND r.owner=u.id AND "
							"(r.access='public' OR r.owner=? OR u.type>?))"
					" UNION "
					"(SELECT id, name FROM rounds, users_to_contests "
						"WHERE user_id=? AND contest_id=id) ORDER BY id"));
				pstmt->setString(1, sim_.session->user_id);
				pstmt->setUInt(2, sim_.session->user_type);
				pstmt->setString(3, sim_.session->user_id);

			} else
				pstmt.reset(sim_.db_conn()->prepareStatement(
					"SELECT id, name FROM rounds "
					"WHERE parent IS NULL AND access='public' ORDER BY id"));

			// List them
			UniquePtr<sql::ResultSet> res(pstmt->executeQuery());
			templ << "<div class=\"contests-list\">\n";

			// Add contest button (admins and teachers only)
			if (sim_.session->state() == Session::OK &&
					sim_.session->user_type < 2)
				templ << "<a class=\"btn\" href=\"/c/add\">Add contest</a>\n";

			while (res->next())
				templ << "<a href=\"/c/" << htmlSpecialChars(res->getString(1))
					<< "\">" << htmlSpecialChars(res->getString(2)) << "</a>\n";
			templ << "</div>\n";

		} catch (const std::exception& e) {
			E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__, __LINE__,
				e.what());

		} catch (...) {
			E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
		}

	// Add contest
	} else if (0 == compareTo(sim_.req_->target, arg_beg, '/', "add"))
		addContest();

	// Other pages which need round id
	else {
		// Extract round id
		string round_id;
		int res_code = strToNum(round_id, sim_.req_->target, arg_beg, '/');
		if (res_code == -1)
			return sim_.error404();

		arg_beg += res_code + 1;

		// Get parent rounds
		delete r_path_;
		r_path_ = getRoundPath(round_id);
		if (r_path_ == NULL)
			return;

		// Check if user forces observer view
		bool admin_view = r_path_->admin_access;
		if (0 == compareTo(sim_.req_->target, arg_beg, '/', "n")) {
			admin_view = false;
			arg_beg += 2;
		}

		// Problem statement
		if (r_path_->type == PROBLEM &&
				0 == compareTo(sim_.req_->target, arg_beg, '/', "statement")) {
			arg_beg += 10;
			// Get statement path
			ConfigFile problem_config;
			problem_config.addVar("statement");
			problem_config.loadConfigFromFile("problems/" +
				r_path_->problem->problem_id + "/config.conf");

			string statement = problem_config.getString("statement");
			// No statement
			if (statement.empty()) {
				TemplateWithMenu templ(*this, "Problems");
				templ << "<h1>Problems</h1>";
				templ.printRoundPath(*r_path_, "problems");
				templ << "<p>This problem has no statement...</p>";
				return;
			}

			if (isSuffix(statement, ".pdf"))
				sim_.resp_.headers["Content-type"] = "application/pdf";
			else if (isSuffix(statement, ".html") ||
					isSuffix(statement, ".htm"))
				sim_.resp_.headers["Content-type"] = "text/html";
			else if (isSuffix(statement, ".txt") || isSuffix(statement, ".md"))
				sim_.resp_.headers["Content-type"] =
					"text/plain; charset=utf-8";

			sim_.resp_.content_type = server::HttpResponse::FILE;
			sim_.resp_.content.clear();
			sim_.resp_.content.append("problems/").
				append(r_path_->problem->problem_id).append("/doc/").
				append(statement);
			return;
		}

		// Add
		if (0 == compareTo(sim_.req_->target, arg_beg, '/', "add")) {
			arg_beg += 4;
			if (r_path_->type == CONTEST)
				return addRound();

			if (r_path_->type == ROUND)
				return addProblem();

			return sim_.error404();
		}

		// Edit
		if (0 == compareTo(sim_.req_->target, arg_beg, '/', "edit")) {
			arg_beg += 5;
			if (r_path_->type == CONTEST)
				return editContest();

			if (r_path_->type == ROUND)
				return editRound();

			return editProblem();
		}

		// Delete
		if (0 == compareTo(sim_.req_->target, arg_beg, '/', "delete")) {
			arg_beg += 7;
			if (r_path_->type == CONTEST)
				return deleteContest();

			if (r_path_->type == ROUND)
				return deleteRound();

			return deleteProblem();
		}

		// Problems
		if (0 == compareTo(sim_.req_->target, arg_beg, '/', "problems")) {
			arg_beg += 9;
			return problems(admin_view);
		}

		// Submit
		if (0 == compareTo(sim_.req_->target, arg_beg, '/', "submit")) {
			arg_beg += 6;
			return submit(admin_view);
		}

		// Submissions
		if (0 == compareTo(sim_.req_->target, arg_beg, '/', "submissions")) {
			arg_beg += 12;
			return submissions(admin_view);
		}

		// Contest dashboard
		TemplateWithMenu templ(*this, "Contest dashboard");

		templ << "<h1>Dashboard</h1>";
		templ.printRoundPath(*r_path_, "");
		templ.printRoundView(*r_path_, false, admin_view);

		if (r_path_->type == PROBLEM)
			templ << "<a class=\"btn\" href=\"/c/" << r_path_->round_id
				<< "/statement\" style=\"margin:5px auto 5px auto\">"
				"View statement</a>\n";
	}
}

void Sim::Contest::addContest() {
	if (sim_.session->open() != Sim::Session::OK || sim_.session->user_type > 1)
		return sim_.error403();

	FormValidator fv(sim_.req_->form_data);
	string name;
	bool is_public = false;

	if (sim_.req_->method == server::HttpRequest::POST) {
		// Validate all fields
		fv.validateNotBlank(name, "name", "Contest name", 128);
		is_public = fv.exist("public");

		// If all fields are ok
		if (fv.noErrors())
			try {
				UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn()->
					prepareStatement("INSERT INTO rounds"
							"(access, name, owner, item) "
						"SELECT ?, ?, ?, MAX(item)+1 FROM rounds "
							"WHERE parent IS NULL"));
				pstmt->setString(1, is_public ? "public" : "private");
				pstmt->setString(2, name);
				pstmt->setString(3, sim_.session->user_id);

				if (pstmt->executeUpdate() != 1)
					throw std::runtime_error("Failed to insert round");

				UniquePtr<sql::Statement> stmt(sim_.db_conn()->
					createStatement());
				UniquePtr<sql::ResultSet> res(stmt->
					executeQuery("SELECT LAST_INSERT_ID()"));

				if (res->next())
					return sim_.redirect("/c/" + res->getString(1));

				return sim_.redirect("/c");

			} catch (const std::exception& e) {
				E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__,
					__LINE__, e.what());

			} catch (...) {
				E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
			}
	}

	Sim::Template templ(sim_, "Add contest", ".body{margin-left:30px}");
	templ << fv.errors() << "<div class=\"form-container\">\n"
			"<h1>Add contest</h1>\n"
			"<form method=\"post\">\n"
				// Name
				"<div class=\"field-group\">\n"
					"<label>Contest name</label>\n"
					"<input type=\"text\" name=\"name\" value=\""
						<< htmlSpecialChars(name) << "\" size=\"24\" "
						"maxlength=\"128\" required>\n"
				"</div>\n"
				// Public
				"<div class=\"field-group\">\n"
					"<label>Public</label>\n"
					"<input type=\"checkbox\" name=\"public\""
						<< (is_public ? " checked" : "") << ">\n"
				"</div>\n"
				"<input class=\"btn\" type=\"submit\" value=\"Add\">\n"
			"</form>\n"
		"</div>\n";
}

void Sim::Contest::addRound() {
	if (!r_path_->admin_access)
		return sim_.error403();

	FormValidator fv(sim_.req_->form_data);
	string name;
	bool is_visible = false;
	string begins, full_results, ends;

	if (sim_.req_->method == server::HttpRequest::POST) {
		// Validate all fields
		fv.validateNotBlank(name, "name", "Round name", 128);
		is_visible = fv.exist("visible");
		fv.validate(begins, "begins", "Begins", isDatetime,
			"Begins: invalid value");
		fv.validate(ends, "ends", "Ends", isDatetime, "Ends: invalid value");
		fv.validate(full_results, "full_results", "Ends", isDatetime,
			"Full_results: invalid value");

		// If all fields are ok
		if (fv.noErrors())
			try {
				UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn()->
					prepareStatement("INSERT INTO rounds"
							"(parent, name, item, visible, begins, ends, "
								"full_results) "
						"SELECT ?, ?, MAX(item)+1, ?, ?, ?, ? FROM rounds "
							"WHERE parent=?"));
				pstmt->setString(1, r_path_->round_id);
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

				pstmt->setString(7, r_path_->round_id);

				if (pstmt->executeUpdate() != 1)
					throw std::runtime_error("Failed to insert round");

				UniquePtr<sql::Statement> stmt(sim_.db_conn()->
					createStatement());
				UniquePtr<sql::ResultSet> res(stmt->
					executeQuery("SELECT LAST_INSERT_ID()"));

				if (res->next())
					return sim_.redirect("/c/" + res->getString(1));

				return sim_.redirect("/c/" + r_path_->round_id);

			} catch (const std::exception& e) {
				E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__,
					__LINE__, e.what());

			} catch (...) {
				E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
			}
	}

	TemplateWithMenu templ(*this, "Add round");
	templ.printRoundPath(*r_path_, "");
	templ << fv.errors() << "<div class=\"form-container\">\n"
		"<h1>Add round</h1>\n"
		"<form method=\"post\">\n"
			// Name
			"<div class=\"field-group\">\n"
				"<label>Round name</label>\n"
				"<input type=\"text\" name=\"name\" value=\""
					<< htmlSpecialChars(name) << "\" size=\"24\" "
					"maxlength=\"128\" required>\n"
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
					<< htmlSpecialChars(begins) << "\" size=\"19\" "
					"maxlength=\"19\">\n"
			"</div>\n"
			// Ends
			"<div class=\"field-group\">\n"
				"<label>Ends</label>\n"
				"<input type=\"text\" name=\"ends\""
					"placeholder=\"yyyy-mm-dd HH:MM:SS\" value=\""
					<< htmlSpecialChars(ends) << "\" size=\"19\" "
					"maxlength=\"19\">\n"
			"</div>\n"
			// Full_results
			"<div class=\"field-group\">\n"
				"<label>Full_results</label>\n"
				"<input type=\"text\" name=\"full_results\""
					"placeholder=\"yyyy-mm-dd HH:MM:SS\" value=\""
					<< htmlSpecialChars(full_results) << "\" size=\"19\" "
					"maxlength=\"19\">\n"
			"</div>\n"
			"<input class=\"btn\" type=\"submit\" value=\"Add\">\n"
		"</form>\n"
	"</div>\n";
}

void Sim::Contest::addProblem() {
	if (!r_path_->admin_access)
		return sim_.error403();

	FormValidator fv(sim_.req_->form_data);
	string name, memory_limit, user_package_file;
	bool force_auto_limit = true;

	if (sim_.req_->method == server::HttpRequest::POST) {
		// Validate all fields
		fv.validate(name, "name", "Problem name", 128);

		fv.validate<bool(*)(const StringView&)>(memory_limit, "memory-limit",
			"Memory limit", isDigit, "Memory limit: invalid value");

		force_auto_limit = fv.exist("force-auto-limit");

		fv.validateNotBlank(user_package_file, "package", "Package");

		// If all fields are OK
		if (fv.noErrors())
			try {
				string package_file = fv.getFilePath("package");

				// Rename package file that it will end with original extension
				string new_package_file = package_file + '.' +
					(isSuffix(user_package_file, ".tar.gz") ? "tar.gz"
						: getExtension(user_package_file));
				if (link(package_file.c_str(), new_package_file.c_str()))
					throw std::runtime_error(string("Error: link() - ") +
						strerror(errno));

				FileRemover file_rm(new_package_file);

				// Create temporary directory for holding package
				char package_tmp_dir[] = "/tmp/sim-problem.XXXXXX";
				if (mkdtemp(package_tmp_dir) == NULL)
					throw std::runtime_error(string("Error: mkdtemp() - ") +
						strerror(errno));

				DirectoryRemover rm_tmp_dir(package_tmp_dir);

				// Construct Conver arguments
				vector<string> args(1, "./conver");
				append(args)(new_package_file)("-o")(package_tmp_dir)("-q");

				if (force_auto_limit)
					args.push_back("-fal");

				if (name.size())
					append(args)("-n")(name);

				if (memory_limit.size())
					append(args)("-m")(memory_limit);

				// Conver stdin, stdout, stderr
				spawn_opts sopt = {
					-1,
					-1,
					getUnlinkedTmpFile()
				};

				if (sopt.new_stderr_fd == -1)
					throw std::runtime_error(
						string("Error: getUnlinkedTmpFile(): ") +
						strerror(errno));

				// Convert package
				if (0 != spawn("./conver", args, &sopt)) {
					// Move offset to begging
					lseek(sopt.new_stderr_fd, 0, SEEK_SET);

					fv.addError("Conver failed - " +
						getFileContents(sopt.new_stderr_fd));
					goto form;
				}

				UniquePtr<sql::Statement> stmt(sim_.db_conn()->
					createStatement());
				UniquePtr<sql::ResultSet> res;

				// 'Transaction' begin
				// Insert problem
				if (1 != stmt->executeUpdate(
						"INSERT INTO problems(name, owner, added) "
						"VALUES('', 0, '')"))
					throw std::runtime_error("Failed to problem");

				// Get problem_id
				res.reset(stmt->executeQuery("SELECT LAST_INSERT_ID()"));
				if (!res->next())
					throw std::runtime_error("Failed to get LAST_INSERT_ID()");

				string problem_id = res->getString(1);

				// Insert round
				if (1 != stmt->executeUpdate(
						"INSERT INTO rounds(name, owner, item) "
						"VALUES('', 0, 0)"))
					throw std::runtime_error("Failed to round");

				// Get round_id
				res.reset(stmt->executeQuery("SELECT LAST_INSERT_ID()"));
				if (!res->next())
					throw std::runtime_error("Failed to get LAST_INSERT_ID()");

				string round_id = res->getString(1);

				// Get problem name
				ConfigFile problem_config;
				problem_config.addVar("name");
				problem_config.loadConfigFromFile(string(package_tmp_dir) +
					"/config.conf");

				name = problem_config.getString("name");
				if (name.empty())
					throw std::runtime_error("Failed to get problem name");

				// Move package folder to problems/
				if (rename(package_tmp_dir, ("problems/" + problem_id).c_str()))
					throw std::runtime_error(string("Error: rename() - ") +
						strerror(errno));

				rm_tmp_dir.reset("problems/" + problem_id);

				// Commit - update problem and round
				UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn()->
					prepareStatement("UPDATE problems p, rounds r,"
							"(SELECT MAX(item)+1 x FROM rounds "
								"WHERE parent=?) t "
						"SET p.name=?, p.owner=?, p.added=?, "
							"parent=?, grandparent=?, r.name=?, item=t.x, "
							"problem_id=? "
						"WHERE p.id=? AND r.id=?"));

				pstmt->setString(1, r_path_->round->id);
				pstmt->setString(2, name);
				pstmt->setString(3, sim_.session->user_id);
				pstmt->setString(4, date("%Y-%m-%d %H:%M:%S"));
				pstmt->setString(5, r_path_->round->id);
				pstmt->setString(6, r_path_->contest->id);
				pstmt->setString(7, name);
				pstmt->setString(8, problem_id);
				pstmt->setString(9, problem_id);
				pstmt->setString(10, round_id);

				if (2 != pstmt->executeUpdate())
					throw std::runtime_error("Failed to update");

				// Cancel folder deletion
				rm_tmp_dir.cancel();

				return sim_.redirect("/c/" + round_id);

			} catch (const std::exception& e) {
				E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__,
					__LINE__, e.what());

			} catch (...) {
				E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
			}
	}

 form:
	TemplateWithMenu templ(*this, "Add problem");
	templ.printRoundPath(*r_path_, "");
	templ << fv.errors() << "<div class=\"form-container\">\n"
			"<h1>Add problem</h1>\n"
			"<form method=\"post\" enctype=\"multipart/form-data\">\n"
				// Name
				"<div class=\"field-group\">\n"
					"<label>Problem name</label>\n"
					"<input type=\"text\" name=\"name\" value=\""
						<< htmlSpecialChars(name) << "\" size=\"24\""
					"maxlength=\"128\" placeholder=\"Detect from config.conf\">"
					"\n"
				"</div>\n"
				// Memory limit
				"<div class=\"field-group\">\n"
					"<label>Memory limit (in kB)</label>\n"
					"<input type=\"text\" name=\"memory-limit\" value=\""
						<< htmlSpecialChars(memory_limit) << "\" size=\"24\""
					"placeholder=\"Detect from config.conf\">"
					"\n"
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
				"<input class=\"btn\" type=\"submit\" value=\"Add\">\n"
			"</form>\n"
		"</div>\n";
}

void Sim::Contest::editContest() {
	if (!r_path_->admin_access)
		return sim_.error403();

	FormValidator fv(sim_.req_->form_data);
	string name, owner;
	bool is_public = false;

	if (sim_.req_->method == server::HttpRequest::POST) {
		// Validate all fields
		fv.validateNotBlank(name, "name", "Contest name", 128);
		fv.validateNotBlank(owner, "owner", "Owner username", 30);
		is_public = fv.exist("public");

		// If all fields are ok
		if (fv.noErrors())
			try {
				UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn()->
					prepareStatement("UPDATE rounds r, (SELECT id FROM users "
							"WHERE username=?) u "
						"SET name=?, owner=u.id, access=? WHERE r.id=?"));
				pstmt->setString(1, owner);
				pstmt->setString(2, name);
				pstmt->setString(3, is_public ? "public" : "private");
				pstmt->setString(4, r_path_->round_id);

				if (pstmt->executeUpdate() == 1) {
					fv.addError("Update successful");
					// Update r_path_
					delete r_path_;
					r_path_ = getRoundPath(r_path_->round_id);
					if (r_path_ == NULL)
						return;
				}

			} catch (const std::exception& e) {
				E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__,
					__LINE__, e.what());

			} catch (...) {
				E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
			}
	}

	// Get contest information
	name = r_path_->contest->name;
	UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn()->
		prepareStatement("SELECT u.username, r.access FROM rounds r, users u "
			"WHERE r.id=? AND r.owner=u.id"));
	pstmt->setString(1, r_path_->round_id);

	UniquePtr<sql::ResultSet> res(pstmt->executeQuery());
	if (res->next()) {
		owner = res->getString(1);
		is_public = res->getString(2) == "public";
	}

	TemplateWithMenu templ(*this, "Edit contest");
	templ.printRoundPath(*r_path_, "");
	templ << fv.errors() << "<div class=\"form-container\">\n"
			"<h1>Edit contest</h1>\n"
			"<form method=\"post\">\n"
				// Name
				"<div class=\"field-group\">\n"
					"<label>Contest name</label>\n"
					"<input type=\"text\" name=\"name\" value=\""
						<< htmlSpecialChars(name) << "\" size=\"24\" "
						"maxlength=\"128\" required>\n"
				"</div>\n"
				// Owner
				"<div class=\"field-group\">\n"
					"<label>Owner username</label>\n"
					"<input type=\"text\" name=\"owner\" value=\""
						<< htmlSpecialChars(owner) << "\" size=\"24\" "
						"maxlength=\"30\" required>\n"
				"</div>\n"
				// Public
				"<div class=\"field-group\">\n"
					"<label>Public</label>\n"
					"<input type=\"checkbox\" name=\"public\""
						<< (is_public ? " checked" : "") << ">\n"
				"</div>\n"
				"<div>\n"
					"<input class=\"btn\" type=\"submit\" value=\"Update\">\n"
					"<a class=\"btn-danger\" style=\"float:right\" href=\"/c/"
						<< r_path_->round_id << "/delete\">Delete contest</a>\n"
				"</div>\n"
			"</form>\n"
		"</div>\n";
}

void Sim::Contest::editRound() {
	if (!r_path_->admin_access)
		return sim_.error403();

	FormValidator fv(sim_.req_->form_data);
	string name;
	bool is_visible = false;
	string begins, full_results, ends;

	if (sim_.req_->method == server::HttpRequest::POST) {
		// Validate all fields
		fv.validateNotBlank(name, "name", "Round name", 128);
		is_visible = fv.exist("visible");
		fv.validate(begins, "begins", "Begins", isDatetime,
			"Begins: invalid value");
		fv.validate(ends, "ends", "Ends", isDatetime, "Ends: invalid value");
		fv.validate(full_results, "full_results", "Ends", isDatetime,
			"Full_results: invalid value");

		// If all fields are ok
		if (fv.noErrors())
			try {
				UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn()->
					prepareStatement("UPDATE rounds SET name=?, visible=?, "
							"begins=?, ends=?, full_results=? WHERE id=?"));
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

				pstmt->setString(6, r_path_->round_id);

				if (pstmt->executeUpdate() == 1) {
					fv.addError("Update successful");
					// Update r_path_
					delete r_path_;
					r_path_ = getRoundPath(r_path_->round_id);
					if (r_path_ == NULL)
						return;
				}

			} catch (const std::exception& e) {
				E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__,
					__LINE__, e.what());

			} catch (...) {
				E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
			}
	}

	// Get round information
	name = r_path_->round->name;
	is_visible = r_path_->round->visible;
	begins = r_path_->round->begins;
	ends = r_path_->round->ends;
	full_results = r_path_->round->full_results;

	TemplateWithMenu templ(*this, "Edit round");
	templ.printRoundPath(*r_path_, "");
	templ << fv.errors() << "<div class=\"form-container\">\n"
			"<h1>Edit round</h1>\n"
			"<form method=\"post\">\n"
				// Name
				"<div class=\"field-group\">\n"
					"<label>Round name</label>\n"
					"<input type=\"text\" name=\"name\" value=\""
						<< htmlSpecialChars(name) << "\" size=\"24\" "
						"maxlength=\"128\" required>\n"
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
						<< htmlSpecialChars(begins) << "\" size=\"19\" "
						"maxlength=\"19\">\n"
				"</div>\n"
				// Ends
				"<div class=\"field-group\">\n"
					"<label>Ends</label>\n"
					"<input type=\"text\" name=\"ends\""
						"placeholder=\"yyyy-mm-dd HH:MM:SS\" value=\""
						<< htmlSpecialChars(ends) << "\" size=\"19\" "
						"maxlength=\"19\">\n"
				"</div>\n"
				// Full_results
				"<div class=\"field-group\">\n"
					"<label>Full_results</label>\n"
					"<input type=\"text\" name=\"full_results\""
						"placeholder=\"yyyy-mm-dd HH:MM:SS\" value=\""
						<< htmlSpecialChars(full_results) << "\" size=\"19\" "
						"maxlength=\"19\">\n"
				"</div>\n"
				"<div>\n"
					"<input class=\"btn\" type=\"submit\" value=\"Update\">\n"
					"<a class=\"btn-danger\" style=\"float:right\" href=\"/c/"
						<< r_path_->round_id << "/delete\">Delete round</a>\n"
				"</div>\n"
			"</form>\n"
		"</div>\n";
}

void Sim::Contest::editProblem() {
	if (!r_path_->admin_access)
		return sim_.error403();

	// Rejudge
	if (0 == compareTo(sim_.req_->target, arg_beg, '/', "rejudge")) {
		try {
			UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn()->
				prepareStatement("UPDATE submissions "
				"SET status='waiting', queued=? WHERE problem_id=?"));
			pstmt->setString(1, date("%Y-%m-%d %H:%M:%S"));
			pstmt->setString(2, r_path_->problem->problem_id);

			pstmt->executeUpdate();
			notifyJudgeServer();

		} catch (const std::exception& e) {
			E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__, __LINE__,
				e.what());

		} catch (...) {
			E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
		}

		return sim_.redirect(sim_.req_->target.substr(0, arg_beg - 1));
	}

	FormValidator fv(sim_.req_->form_data);
	string round_name, name, tag, memory_limit;

	if (sim_.req_->method == server::HttpRequest::POST) {
		// Validate all fields
		fv.validate(round_name, "round-name", "Problem round name", 128);

		fv.validate(name, "name", "Problem name", 128);

		fv.validate(tag, "tag", "Problem tag", 4);

		fv.validateNotBlank<bool(*)(const StringView&)>(memory_limit,
			"memory-limit", "Memory limit", isDigit,
			"Memory limit: invalid value");

		// If all fields are ok
		if (fv.noErrors())
			try {
				// Update problem config
				ProblemConfig pconfig; // TODO: ProblemConfig is too heavy here
				pconfig.loadConfig("problems/" + r_path_->problem->problem_id);

				pconfig.name = name;
				pconfig.tag = tag;
				pconfig.memory_limit = strtoull(memory_limit);

				if (putFileContents("problems/" + r_path_->problem->problem_id
						+ "/config.conf", pconfig.dump()) == size_t(-1))
					throw std::runtime_error("Failed to update problem " +
						r_path_->problem->problem_id + " config");

				// Update database
				UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn()->
					prepareStatement("UPDATE rounds r, problems p "
						"SET r.name=?, p.name=? WHERE r.id=? AND p.id=?"));
				pstmt->setString(1, round_name);
				pstmt->setString(2, name);
				pstmt->setString(3, r_path_->round_id);
				pstmt->setString(4, r_path_->problem->problem_id);

				if (pstmt->executeUpdate() > 0) {
					// Update r_path_
					delete r_path_;
					r_path_ = getRoundPath(r_path_->round_id);
					if (r_path_ == NULL)
						return;
				}

			} catch (const std::exception& e) {
				E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__,
					__LINE__, e.what());

			} catch (...) {
				E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
			}
	}

	// Get problem information
	round_name = r_path_->problem->name;
	ConfigFile pconfig;
	pconfig.addVar("name");
	pconfig.addVar("tag");
	pconfig.addVar("memory_limit");

	pconfig.loadConfigFromFile("problems/" + r_path_->problem->problem_id +
		"/config.conf");
	name = pconfig.getString("name");
	tag = pconfig.getString("tag");
	memory_limit = pconfig.getString("memory_limit");

	TemplateWithMenu templ(*this, "Edit problem");
	templ.printRoundPath(*r_path_, "");
	templ << fv.errors() << "<div class=\"right-flow\" style=\"width:85%\">"
			"<a class=\"btn-small\" href=\""
				<< sim_.req_->target.substr(0, arg_beg - 1)
				<< "/rejudge\">Rejudge all submissions</a>\n"
		"</div>\n"
		"<div class=\"form-container\">\n"
			"<h1>Edit problem</h1>\n"
			"<form method=\"post\">\n"
				// Problem round name
				"<div class=\"field-group\">\n"
					"<label>Problem round name</label>\n"
					"<input type=\"text\" name=\"round-name\" value=\""
						<< htmlSpecialChars(round_name) << "\" size=\"24\" "
						"maxlength=\"128\" required>\n"
				"</div>\n"
				// Problem name
				"<div class=\"field-group\">\n"
					"<label>Problem name</label>\n"
					"<input type=\"text\" name=\"name\" value=\""
						<< htmlSpecialChars(name) << "\" size=\"24\" "
						"maxlength=\"128\" required>\n"
				"</div>\n"
				// Tag
				"<div class=\"field-group\">\n"
					"<label>Problem tag</label>\n"
					"<input type=\"text\" name=\"tag\" value=\""
						<< htmlSpecialChars(tag) << "\" size=\"24\" "
						"maxlength=\"4\" required>\n"
				"</div>\n"
				// TODO: Checker
				// Memory limit
				"<div class=\"field-group\">\n"
					"<label>Memory limit (in kB)</label>\n"
					"<input type=\"text\" name=\"memory-limit\" value=\""
						<< htmlSpecialChars(memory_limit) << "\" size=\"24\" "
						"required>\n"
				"</div>\n"
				// TODO: Main solution
				"<div>\n"
					"<input class=\"btn\" type=\"submit\" value=\"Update\">\n"
					"<a class=\"btn-danger\" style=\"float:right\" href=\"/c/"
						<< r_path_->round_id << "/delete\">Delete problem</a>\n"
				"</div>\n"
			"</form>\n"
		"</div>\n";
}

void Sim::Contest::deleteContest() {
	if (!r_path_->admin_access)
		return sim_.error403();

	FormValidator fv(sim_.req_->form_data);
	if (sim_.req_->method == server::HttpRequest::POST && fv.exist("delete"))
		try {
			// Delete submissions
			UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn()->
				prepareStatement("DELETE FROM submissions "
					"WHERE contest_round_id=?"));
			pstmt->setString(1, r_path_->round_id);
			pstmt->executeUpdate();

			// Delete from users_to_contests
			pstmt.reset(sim_.db_conn()->prepareStatement(
				"DELETE FROM users_to_contests WHERE contest_id=?"));
			pstmt->setString(1, r_path_->round_id);
			pstmt->executeUpdate();

			// Delete rounds
			pstmt.reset(sim_.db_conn()->prepareStatement("DELETE FROM rounds "
				"WHERE id=? OR parent=? OR grandparent=?"));
			pstmt->setString(1, r_path_->round_id);
			pstmt->setString(2, r_path_->round_id);
			pstmt->setString(3, r_path_->round_id);

			if (pstmt->executeUpdate() > 0)
				return sim_.redirect("/c");

		} catch (const std::exception& e) {
			E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__, __LINE__,
				e.what());

		} catch (...) {
			E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
		}

	TemplateWithMenu templ(*this, "Delete contest");
	templ.printRoundPath(*r_path_, "");
	templ << fv.errors() << "<div class=\"form-container\">\n"
		"<h1>Delete contest</h1>\n"
		"<form method=\"post\">\n"
			"<label class=\"field\">Are you sure to delete contest "
				"<a href=\"/c/" << r_path_->round_id << "\">"
				<< htmlSpecialChars(r_path_->contest->name) << "</a>, all "
				"subrounds and submissions?</label>\n"
			"<div class=\"submit-yes-no\">\n"
				"<button class=\"btn-danger\" type=\"submit\" name=\"delete\">"
					"Yes, I'm sure</button>\n"
				"<a class=\"btn\" href=\"/c/" << r_path_->round_id << "/edit\">"
					"No, go back</a>\n"
			"</div>\n"
		"</form>\n"
	"</div>\n";
}

void Sim::Contest::deleteRound() {
	if (!r_path_->admin_access)
		return sim_.error403();

	FormValidator fv(sim_.req_->form_data);
	if (sim_.req_->method == server::HttpRequest::POST && fv.exist("delete"))
		try {
			// Delete submissions
			UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn()->
				prepareStatement("DELETE FROM submissions "
					"WHERE parent_round_id=?"));
			pstmt->setString(1, r_path_->round_id);
			pstmt->executeUpdate();

			// Delete rounds
			pstmt.reset(sim_.db_conn()->prepareStatement(
				"DELETE FROM rounds WHERE id=? OR parent=?"));
			pstmt->setString(1, r_path_->round_id);
			pstmt->setString(2, r_path_->round_id);

			if (pstmt->executeUpdate() > 0)
				return sim_.redirect("/c/" + r_path_->contest->id);

		} catch (const std::exception& e) {
			E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__, __LINE__,
				e.what());

		} catch (...) {
			E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
		}

	TemplateWithMenu templ(*this, "Delete round");
	templ.printRoundPath(*r_path_, "");
	templ << fv.errors() << "<div class=\"form-container\">\n"
		"<h1>Delete round</h1>\n"
		"<form method=\"post\">\n"
			"<label class=\"field\">Are you sure to delete round <a href=\"/c/"
				<< r_path_->round_id << "\">"
				<< htmlSpecialChars(r_path_->round->name) << "</a>, all "
				"subrounds and submissions?</label>\n"
			"<div class=\"submit-yes-no\">\n"
				"<button class=\"btn-danger\" type=\"submit\" name=\"delete\">"
					"Yes, I'm sure</button>\n"
				"<a class=\"btn\" href=\"/c/" << r_path_->round_id << "/edit\">"
					"No, go back</a>\n"
			"</div>\n"
		"</form>\n"
	"</div>\n";
}

void Sim::Contest::deleteProblem() {
	if (!r_path_->admin_access)
		return sim_.error403();

	FormValidator fv(sim_.req_->form_data);
	if (sim_.req_->method == server::HttpRequest::POST && fv.exist("delete"))
		try {
			// Delete submissions
			UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn()->
				prepareStatement("DELETE FROM submissions WHERE round_id=?"));
			pstmt->setString(1, r_path_->round_id);
			pstmt->executeUpdate();

			// Delete problem round
			pstmt.reset(sim_.db_conn()->prepareStatement(
				"DELETE FROM rounds WHERE id=?"));
			pstmt->setString(1, r_path_->round_id);

			if (pstmt->executeUpdate() > 0)
				return sim_.redirect("/c/" + r_path_->round->id);

		} catch (const std::exception& e) {
			E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__, __LINE__,
				e.what());

		} catch (...) {
			E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__, __LINE__);
		}

	TemplateWithMenu templ(*this, "Delete problem");
	templ.printRoundPath(*r_path_, "");
	templ << fv.errors() << "<div class=\"form-container\">\n"
		"<h1>Delete problem</h1>\n"
		"<form method=\"post\">\n"
			"<label class=\"field\">Are you sure to delete problem <a href=\"/c/"
				<< r_path_->round_id << "\">"
				<< htmlSpecialChars(r_path_->problem->name) << "</a> and all "
				"its submissions?</label>\n"
			"<div class=\"submit-yes-no\">\n"
				"<button class=\"btn-danger\" type=\"submit\" name=\"delete\">"
					"Yes, I'm sure</button>\n"
				"<a class=\"btn\" href=\"/c/" << r_path_->round_id << "/edit\">"
					"No, go back</a>\n"
			"</div>\n"
		"</form>\n"
	"</div>\n";
}

void Sim::Contest::problems(bool admin_view) {
	TemplateWithMenu templ(*this, "Problems");
	templ << "<h1>Problems</h1>";
	templ.printRoundPath(*r_path_, "problems");
	templ.printRoundView(*r_path_, true, admin_view);
}

void Sim::Contest::submit(bool admin_view) {
	if (sim_.session->open() != Sim::Session::OK)
		return sim_.redirect("/login" + sim_.req_->target);

	FormValidator fv(sim_.req_->form_data);

	if (sim_.req_->method == server::HttpRequest::POST) {
		string solution, problem_round_id;

		// Validate all fields
		fv.validateNotBlank(solution, "solution", "Solution file field");

		fv.validateNotBlank(problem_round_id, "round-id", "Problem");

		if (!isInteger(problem_round_id))
			fv.addError("Wrong problem round id");

		// If all fields are ok
		// TODO: "transaction"
		if (fv.noErrors()) {
			UniquePtr<RoundPath> path;
			const RoundPath* problem_r_path = r_path_;

			if (r_path_->type != PROBLEM) {
				// Get parent rounds of problem round
				path.reset(getRoundPath(problem_round_id));
				if (path.isNull())
					return;

				if (path->type != PROBLEM) {
					fv.addError("Wrong problem round id");
					goto form;
				}

				problem_r_path = path.get();
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
				UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn()->
					prepareStatement("INSERT INTO submissions "
						"(user_id, problem_id, round_id, parent_round_id, "
							"contest_round_id, submit_time, queued) "
						"VALUES(?, ?, ?, ?, ?, ?, ?)"));
				pstmt->setString(1, sim_.session->user_id);
				pstmt->setString(2, problem_r_path->problem->problem_id);
				pstmt->setString(3, problem_r_path->problem->id);
				pstmt->setString(4, problem_r_path->round->id);
				pstmt->setString(5, problem_r_path->contest->id);
				pstmt->setString(6, current_date);
				pstmt->setString(7, current_date);

				if (1 != pstmt->executeUpdate()) {
					fv.addError("Database error - failed to insert submission");
					goto form;
				}

				// Get inserted submission id
				UniquePtr<sql::Statement> stmt(sim_.db_conn()->
					createStatement());
				UniquePtr<sql::ResultSet> res(stmt->
					executeQuery("SELECT LAST_INSERT_ID()"));

				if (!res->next()) {
					fv.addError("Database error - failed to get inserted "
						"submission id");
					goto form;
				}

				string submission_id = res->getString(1);

				// Copy solution
				copy(solution_tmp_path, "solutions/" + submission_id + ".cpp");

				// Change submission status to 'waiting'
				stmt->executeUpdate("UPDATE submissions SET status='waiting' "
					"WHERE id=" + submission_id);

				notifyJudgeServer();

				return sim_.redirect("/s/" + submission_id);

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
	TemplateWithMenu templ(*this, "Submit a solution");
	templ.printRoundPath(*r_path_, "");
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
		if (r_path_->type == CONTEST) {
			// Select subrounds
			// Admin -> All problems from all subrounds
			// Normal -> All problems from subrounds which have begun and
			// have not ended
			UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn()->
				prepareStatement(admin_view ?
					"SELECT id, name FROM rounds WHERE parent=? ORDER BY item"
					: "SELECT id, name FROM rounds WHERE parent=? "
						"AND (begins IS NULL OR begins<=?) "
						"AND (ends IS NULL OR ?<ends) ORDER BY item"));
			pstmt->setString(1, r_path_->contest->id);
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
			pstmt.reset(sim_.db_conn()->prepareStatement(
				"SELECT id, parent, name FROM rounds WHERE grandparent=? "
				"ORDER BY item"));
			pstmt->setString(1, r_path_->contest->id);
			res.reset(pstmt->executeQuery());

			// (round_id, problems)
			std::map<string, vector<Problem> > problems_table;

			// Fill problems with all subrounds
			for (size_t i = 0; i < subrounds.size(); ++i)
				problems_table[subrounds[i].id];

			// Collect results
			while (res->next()) {
				// Get reference to proper vector<Problem>
				__typeof(problems_table.begin()) it =
						problems_table.find(res->getString(2));
				// If problem parent is not visible or database error
				if (it == problems_table.end())
					continue; // Ignore

				vector<Problem>& prob = it->second;
				prob.push_back(Problem());
				prob.back().id = res->getString(1);
				prob.back().parent = res->getString(2);
				prob.back().name = res->getString(3);
			}

			// For each subround list all problems
			foreach (subround, subrounds) {
				vector<Problem>& prob = problems_table[subround->id];

				foreach (problem, prob)
					append(buffer) << "<option value=\"" << problem->id << "\">"
						<< htmlSpecialChars(problem->name) << " ("
						<< htmlSpecialChars(subround->name) << ")</option>\n";
			}

		// Admin -> All problems
		// Normal -> if round has begun and has not ended
		} else if (r_path_->type == ROUND && (admin_view || (
				r_path_->round->begins <= current_date && // "" <= everything
				(r_path_->round->ends.empty() ||
					current_date < r_path_->round->ends)))) {
			// Select problems
			UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn()->
				prepareStatement("SELECT id, name FROM rounds "
					"WHERE parent=? ORDER BY item"));
			pstmt->setString(1, r_path_->round->id);

			// List problems
			UniquePtr<sql::ResultSet> res(pstmt->executeQuery());
			while (res->next())
				append(buffer) << "<option value=\"" << res->getString(1)
					<< "\">" << htmlSpecialChars(res->getString(2)) << " ("
					<< htmlSpecialChars(r_path_->round->name) << ")</option>\n";

		// Admin -> Current problem
		// Normal -> if parent round has begun and has not ended
		} else if (r_path_->type == PROBLEM && (admin_view || (
				r_path_->round->begins <= current_date && // "" <= everything
				(r_path_->round->ends.empty() ||
					current_date < r_path_->round->ends)))) {
			append(buffer) << "<option value=\"" << r_path_->problem->id
				<< "\">" << htmlSpecialChars(r_path_->problem->name) << " ("
				<< htmlSpecialChars(r_path_->round->name) << ")</option>\n";
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
					"<input class=\"btn\" type=\"submit\" value=\"Submit\">\n"
				"</form>\n"
			"</div>\n";

	else
		templ << "<p>There are no problems for which you can submit a "
			"solution...</p>";
}

void Sim::Contest::submission() {
	if (sim_.session->open() != Session::OK)
		return sim_.redirect("/login" + sim_.req_->target);

	arg_beg = 3;

	// Extract round id
	string submission_id;
	int res_code = strToNum(submission_id, sim_.req_->target, arg_beg, '/');
	if (res_code == -1)
		return sim_.error404();

	arg_beg += res_code + 1;

	try {
		// Get submission
		UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn()->
			prepareStatement("SELECT user_id, round_id, submit_time, status, "
					"score, name FROM submissions s, problems p "
				"WHERE s.id=? AND s.problem_id=p.id"));
		pstmt->setString(1, submission_id);

		UniquePtr<sql::ResultSet> res(pstmt->executeQuery());
		if (!res->next())
			return sim_.error404();

		string submission_user_id = res->getString(1);
		string round_id = res->getString(2);
		string submit_time = res->getString(3);
		string submission_status = res->getString(4);
		string score = res->getString(5);
		string problem_name = res->getString(6);

		// Get parent rounds
		delete r_path_;
		r_path_ = getRoundPath(round_id);
		if (r_path_ == NULL)
			return;

		if (!r_path_->admin_access &&
				sim_.session->user_id != submission_user_id)
			return sim_.error403();

		// Check if user forces observer view
		bool admin_view = r_path_->admin_access;
		if (0 == compareTo(sim_.req_->target, arg_beg, '/', "n")) {
			admin_view = false;
			arg_beg += 2;
		}

		// Delete
		if (0 == compareTo(sim_.req_->target, arg_beg, '/', "delete")) {
			if (!admin_view)
				return sim_.error403();

			FormValidator fv(sim_.req_->form_data);
			if (sim_.req_->method == server::HttpRequest::POST
					&& fv.exist("delete"))
				try {
					// Delete submission
					pstmt.reset(sim_.db_conn()->prepareStatement(
						"DELETE FROM submissions WHERE id=?"));
					pstmt->setString(1, submission_id);

					if (pstmt->executeUpdate() == 0) {
						fv.addError("Deletion failed");
						goto delete_form;
					}

					// Restore `final` status
					// TODO: try to simplify following UPDATE (see judge
					// machine submission update)
					pstmt.reset(sim_.db_conn()->prepareStatement(
						"UPDATE submissions SET final=IF(id IN (SELECT id FROM "
							"(SELECT id FROM submissions "
								"WHERE user_id=? AND round_id=? AND "
									"status IN ('ok', 'error') "
								"ORDER BY id DESC LIMIT 1) x), 1, 0) "
						"WHERE id IN "
							"(SELECT id FROM "
								"(SELECT id FROM submissions "
									"WHERE user_id=? AND round_id=? AND "
										"status IN ('ok', 'error') "
									"ORDER BY id DESC LIMIT 1) x) "
							"OR user_id=? AND round_id=? AND final=1"));
					pstmt->setString(1, submission_user_id);
					pstmt->setString(2, round_id);
					pstmt->setString(3, submission_user_id);
					pstmt->setString(4, round_id);
					pstmt->setString(5, submission_user_id);
					pstmt->setString(6, round_id);
					pstmt->executeUpdate();

					return sim_.redirect("/c/" + round_id + "/submissions");

				} catch (const std::exception& e) {
					E("\e[31mCaught exception: %s:%d\e[m - %s\n", __FILE__,
						__LINE__, e.what());

				} catch (...) {
					E("\e[31mCaught exception: %s:%d\e[m\n", __FILE__,
						__LINE__);
				}

		delete_form:
			TemplateWithMenu templ(*this, "Delete submission");
			templ.printRoundPath(*r_path_, "");
			templ << fv.errors() << "<div class=\"form-container\">\n"
				"<h1>Delete submission</h1>\n"
				"<form method=\"post\">\n"
					"<label class=\"field\">Are you sure to delete submission "
					"<a href=\"/c/" << submission_id << "\">" << submission_id
						<< "</a>?</label>\n"
					"<div class=\"submit-yes-no\">\n"
						"<button class=\"btn-danger\" type=\"submit\" "
							"name=\"delete\">Yes, I'm sure</button>\n"
						"<a class=\"btn\" href=\""
							<< sim_.req_->target.substr(0, arg_beg - 1) << "\">"
							"No, go back</a>\n"
					"</div>\n"
				"</form>\n"
			"</div>\n";

			return;
		}

		// Rejudge
		if (0 == compareTo(sim_.req_->target, arg_beg, '/', "rejudge")) {
			if (!admin_view)
				return sim_.error403();

			pstmt.reset(sim_.db_conn()->prepareStatement("UPDATE submissions "
				"SET status='waiting', queued=? WHERE id=?"));
			pstmt->setString(1, date("%Y-%m-%d %H:%M:%S"));
			pstmt->setString(2, submission_id);

			pstmt->executeUpdate();
			notifyJudgeServer();

			return sim_.redirect(sim_.req_->target.substr(0, arg_beg - 1));
		}

		// Download solution
		if (0 == compareTo(sim_.req_->target, arg_beg, '/', "download")) {
			sim_.resp_.headers["Content-type"] = "application/text";
			sim_.resp_.headers["Content-Disposition"] = "attchment; filename=" +
				submission_id + ".cpp";

			sim_.resp_.content = "solutions/" + submission_id + ".cpp";
			sim_.resp_.content_type = server::HttpResponse::FILE;

			return;
		}

		TemplateWithMenu templ(*this, "Submission " + submission_id);
		templ.printRoundPath(*r_path_, "");

		// View source
		if (0 == compareTo(sim_.req_->target, arg_beg, '/', "source")) {
			vector<string> args;
			append(args)("./CTH")("solutions/" + submission_id + ".cpp");

			spawn_opts sopt = {
				-1,
				getUnlinkedTmpFile(),
				-1
			};

			if (sopt.new_stdout_fd < 0)
				return sim_.error500();

			// Run CTH
			spawn(args[0], args, &sopt);
			// Print source code
			lseek(sopt.new_stdout_fd, 0, SEEK_SET);
			templ << getFileContents(sopt.new_stdout_fd);

			Closer(sopt.new_stdout_fd);
			return;
		}

		templ << "<div class=\"submission-info\">\n"
			"<div>\n"
				"<h1>Submission " << submission_id << "</h1>\n"
				"<div>\n"
					"<a class=\"btn-small\" href=\""
						<< sim_.req_->target.substr(0, arg_beg - 1)
						<< "/source\">View source</a>\n"
					"<a class=\"btn-small\" href=\""
						<< sim_.req_->target.substr(0, arg_beg - 1)
						<< "/download\">Download</a>\n";
		if (admin_view)
			templ << "<a class=\"btn-small\" href=\""
					<< sim_.req_->target.substr(0, arg_beg - 1)
					<< "/rejudge\">Rejudge</a>\n"
				"<a class=\"btn-small-danger\" href=\""
					<< sim_.req_->target.substr(0, arg_beg - 1)
					<< "/delete\">Delete</a>\n";
		templ << "</div>\n"
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
							r_path_->round->full_results.empty() ||
							r_path_->round->full_results <=
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
			if (admin_view || r_path_->round->full_results.empty() ||
					r_path_->round->full_results <= date("%Y-%m-%d %H:%M:%S"))
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

void Sim::Contest::submissions(bool admin_view) {
	if (sim_.session->open() != Sim::Session::OK)
		return sim_.redirect("/login" + sim_.req_->target);

	TemplateWithMenu templ(*this, "Submissions");
	templ << "<h1>Submissions</h1>";
	templ.printRoundPath(*r_path_, "submissions");

	templ << "<h3>Submission queue size: ";
	try {
		UniquePtr<sql::Statement> stmt(sim_.db_conn()->createStatement());
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
		string param_column = (r_path_->type == CONTEST ? "contest_round_id"
			: (r_path_->type == ROUND ? "parent_round_id" : "round_id"));

		UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn()->
			prepareStatement(admin_view ? "SELECT s.id, s.submit_time, r2.id, "
					"r2.name, r.id, r.name, s.status, s.score, s.final, "
					"s.user_id, u.username "
				"FROM submissions s, rounds r, rounds r2, users u "
				"WHERE s." + param_column + "=? AND s.round_id=r.id "
					"AND r.parent=r2.id AND s.user_id=u.id ORDER BY s.id DESC"
			: "SELECT s.id, s.submit_time, r2.id, r2.name, r.id, r.name, "
					"s.status, s.score, s.final, r2.full_results "
				"FROM submissions s, rounds r, rounds r2 "
				"WHERE s." + param_column + "=? AND s.round_id=r.id "
					"AND r.parent=r2.id AND s.user_id=? ORDER BY s.id DESC"));
		pstmt->setString(1, r_path_->round_id);
		if (!admin_view)
			pstmt->setString(2, sim_.session->user_id);

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
				templ << "<td><a href=\"/u/" << res->getString(10) << "\">"
						<< htmlSpecialChars(res->getString(11)) << "</a></td>";
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

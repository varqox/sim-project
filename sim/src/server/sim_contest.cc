#include "form_validator.h"
#include "sim_contest_utility.h"
#include "sim_session.h"

#include "../simlib/include/config_file.h"
#include "../simlib/include/filesystem.h"
#include "../simlib/include/logger.h"
#include "../simlib/include/process.h"
#include "../simlib/include/sim_problem.h"
#include "../simlib/include/time.h"

#include <cppconn/prepared_statement.h>

using std::pair;
using std::string;
using std::vector;

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

				pstmt.reset(sim_.db_conn->prepareStatement(
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
				pstmt.reset(sim_.db_conn->prepareStatement(
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
				templ << "<a href=\"/c/"
					<< htmlSpecialChars(StringView(res->getString(1))) << "\">"
					<< htmlSpecialChars(StringView(res->getString(2)))
					<< "</a>\n";
			templ << "</div>\n";

		} catch (const std::exception& e) {
			error_log("Caught exception: ", __FILE__, ':', toString(__LINE__),
				" - ", e.what());
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
		r_path_.reset(getRoundPath(round_id));
		if (r_path_.isNull()) {
			error_log("Corrupt hierarchy of rounds (id: ", round_id, ")");
			return sim_.error500();
		}

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

		// Ranking
		if (0 == compareTo(sim_.req_->target, arg_beg, '/', "ranking")) {
			arg_beg += 8;
			return ranking(admin_view);
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
	bool is_public = false, show_ranking = false;

	if (sim_.req_->method == server::HttpRequest::POST) {
		// Validate all fields
		fv.validateNotBlank(name, "name", "Contest name", 128);
		is_public = fv.exist("public");
		// Only admins can create public contests
		if (sim_.session->user_type > 0) {
			is_public = false;
			fv.addError("Only admins can create public contests");
		}
		show_ranking = fv.exist("show-ranking");

		// If all fields are ok
		if (fv.noErrors())
			try {
				UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn->
					prepareStatement("INSERT INTO rounds"
							"(access, name, owner, item, show_ranking) "
						"SELECT ?, ?, ?, MAX(item)+1, ? FROM rounds "
							"WHERE parent IS NULL"));
				pstmt->setString(1, is_public ? "public" : "private");
				pstmt->setString(2, name);
				pstmt->setString(3, sim_.session->user_id);
				pstmt->setBoolean(4, show_ranking);

				if (pstmt->executeUpdate() != 1)
					throw std::runtime_error("Failed to insert round");

				UniquePtr<sql::Statement> stmt(sim_.db_conn->
					createStatement());
				UniquePtr<sql::ResultSet> res(stmt->
					executeQuery("SELECT LAST_INSERT_ID()"));

				if (res->next())
					return sim_.redirect("/c/" + res->getString(1));

				return sim_.redirect("/c");

			} catch (const std::exception& e) {
				error_log("Caught exception: ", __FILE__, ':',
					toString(__LINE__), " - ", e.what());
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
						<< (is_public ? " checked" : "")
						<< (sim_.session->user_type > 0 ? " disabled" : "")
						<< ">\n"
				"</div>\n"
				// Show ranking
				"<div class=\"field-group\">\n"
					"<label>Show ranking</label>\n"
					"<input type=\"checkbox\" name=\"show-ranking\""
						<< (show_ranking ? " checked" : "") << ">\n"
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
				UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn->
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

				UniquePtr<sql::Statement> stmt(sim_.db_conn->
					createStatement());
				UniquePtr<sql::ResultSet> res(stmt->
					executeQuery("SELECT LAST_INSERT_ID()"));

				if (res->next())
					return sim_.redirect("/c/" + res->getString(1));

				return sim_.redirect("/c/" + r_path_->round_id);

			} catch (const std::exception& e) {
				error_log("Caught exception: ", __FILE__, ':',
					toString(__LINE__), " - ", e.what());
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
	string name, memory_limit, user_package_file, time_limit;
	bool force_auto_limit = true;

	if (sim_.req_->method == server::HttpRequest::POST) {
		// Validate all fields
		fv.validate(name, "name", "Problem name", 128);

		fv.validate<bool(*)(const StringView&)>(memory_limit, "memory-limit",
			"Memory limit", isDigit, "Memory limit: invalid value");

		fv.validate<bool(*)(const StringView&)>(time_limit, "time-limit",
			"Time limit", isReal, "Time limit: invalid value");
		unsigned long long tl = round(strtod(time_limit.c_str(), nullptr) *
			1000000LL); // Time limit in usec
		if (time_limit.size() && tl == 0)
			fv.addError("Global time limit cannot be lower than 0.000001");

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
				if (mkdtemp(package_tmp_dir) == nullptr)
					throw std::runtime_error(string("Error: mkdtemp() - ") +
						strerror(errno));

				DirectoryRemover rm_tmp_dir(package_tmp_dir);

				// Construct Conver arguments
				vector<string> args(1, "./conver");
				append(args)(new_package_file)("-o")(package_tmp_dir);

				if (force_auto_limit)
					args.push_back("-fal");

				if (name.size())
					append(args)("-n")(name);

				if (memory_limit.size())
					append(args)("-m")(memory_limit);

				if (time_limit.size())
					append(args)("-tl")(toString(tl));

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

				UniquePtr<sql::Statement> stmt(sim_.db_conn->
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
				problem_config.addVar("tag");
				problem_config.loadConfigFromFile(string(package_tmp_dir) +
					"/config.conf");

				name = problem_config.getString("name");
				if (name.empty())
					throw std::runtime_error("Failed to get problem name");

				string tag = problem_config.getString("tag");

				// Move package folder to problems/
				if (move(package_tmp_dir, ("problems/" + problem_id).c_str()))
					throw std::runtime_error(string("Error: move() - ") +
						strerror(errno));

				rm_tmp_dir.reset("problems/" + problem_id);

				// Commit - update problem and round
				UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn->
					prepareStatement("UPDATE problems p, rounds r,"
							"(SELECT MAX(item)+1 x FROM rounds "
								"WHERE parent=?) t "
						"SET p.name=?, p.tag=?, p.owner=?, p.added=?, "
							"parent=?, grandparent=?, r.name=?, item=t.x, "
							"problem_id=? "
						"WHERE p.id=? AND r.id=?"));

				pstmt->setString(1, r_path_->round->id);
				pstmt->setString(2, name);
				pstmt->setString(3, tag);
				pstmt->setString(4, sim_.session->user_id);
				pstmt->setString(5, date("%Y-%m-%d %H:%M:%S"));
				pstmt->setString(6, r_path_->round->id);
				pstmt->setString(7, r_path_->contest->id);
				pstmt->setString(8, name);
				pstmt->setString(9, problem_id);
				pstmt->setString(10, problem_id);
				pstmt->setString(11, round_id);

				if (2 != pstmt->executeUpdate())
					throw std::runtime_error("Failed to update");

				// Cancel folder deletion
				rm_tmp_dir.cancel();

				return sim_.redirect("/c/" + round_id);

			} catch (const std::exception& e) {
				error_log("Caught exception: ", __FILE__, ':',
					toString(__LINE__), " - ", e.what());
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
					"<label>Memory limit [kB]</label>\n"
					"<input type=\"text\" name=\"memory-limit\" value=\""
						<< htmlSpecialChars(memory_limit) << "\" size=\"24\""
					"placeholder=\"Detect from config.conf\">"
					"\n"
				"</div>\n"
				// Global time limit
				"<div class=\"field-group\">\n"
					"<label>Global time limit [s] (for each test)</label>\n"
					"<input type=\"text\" name=\"time-limit\" value=\""
						<< htmlSpecialChars(time_limit) << "\" size=\"24\""
					"placeholder=\"No global time limit\">"
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
	bool is_public, show_ranking;

	if (sim_.req_->method == server::HttpRequest::POST) {
		// Validate all fields
		fv.validateNotBlank(name, "name", "Contest name", 128);
		fv.validateNotBlank(owner, "owner", "Owner username", 30);
		is_public = fv.exist("public");
		show_ranking = fv.exist("show-ranking");

		try {
			UniquePtr<sql::PreparedStatement> pstmt;
			// Check if user has the ability to make contest public
			if (is_public && sim_.session->user_type > 0) {
				pstmt.reset(sim_.db_conn->prepareStatement(
					"SELECT access FROM rounds WHERE id=?"));
				pstmt->setString(1, r_path_->round_id);

				UniquePtr<sql::ResultSet> res(pstmt->executeQuery());
				if (!res->next())
					throw std::runtime_error("Failed to get contest access "
						"field");

				// Only admins can make contest public
				if (res->getString(1) == "private") {
					is_public = false;
					fv.addError("Only admins can make contest public");
				}
			}

			// If all fields are ok
			if (fv.noErrors()) {
				pstmt.reset(sim_.db_conn->prepareStatement(
					"UPDATE rounds r, "
						"(SELECT id FROM users WHERE username=?) u "
					"SET name=?, owner=u.id, access=?, show_ranking=? "
					"WHERE r.id=?"));
				pstmt->setString(1, owner);
				pstmt->setString(2, name);
				pstmt->setString(3, is_public ? "public" : "private");
				pstmt->setBoolean(4, show_ranking);
				pstmt->setString(5, r_path_->round_id);

				if (pstmt->executeUpdate() == 1) {
					fv.addError("Update successful");
					// Update r_path_
					r_path_.reset(getRoundPath(r_path_->round_id));
					if (r_path_.isNull()) {
						error_log("Corrupt hierarchy of rounds (id: ",
							r_path_->round_id, ")");
						return sim_.error500();
					}
				}
			}

		} catch (const std::exception& e) {
			error_log("Caught exception: ", __FILE__, ':',
				toString(__LINE__), " - ", e.what());
		}
	}

	// Get contest information
	UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn->
		prepareStatement("SELECT u.username, r.access FROM rounds r, users u "
			"WHERE r.id=? AND r.owner=u.id"));
	pstmt->setString(1, r_path_->round_id);

	UniquePtr<sql::ResultSet> res(pstmt->executeQuery());
	if (!res->next())
		throw std::runtime_error(concat(__PRETTY_FUNCTION__, ": Failed to get "
			"contest and owner info"));

	name = r_path_->contest->name;
	owner = res->getString(1);
	is_public = res->getString(2) == "public";
	show_ranking = r_path_->contest->show_ranking;

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
						<< (is_public ? " checked"
							: (sim_.session->user_type > 0 ? " disabled" : ""))
						<< ">\n"
				"</div>\n"
				// Show ranking
				"<div class=\"field-group\">\n"
					"<label>Show ranking</label>\n"
					"<input type=\"checkbox\" name=\"show-ranking\""
						<< (show_ranking ? " checked" : "") << ">\n"
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
				UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn->
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
					r_path_.reset(getRoundPath(r_path_->round_id));
					if (r_path_.isNull()) {
						error_log("Corrupt hierarchy of rounds (id: ",
							r_path_->round_id, ")");
						return sim_.error500();
					}
				}

			} catch (const std::exception& e) {
				error_log("Caught exception: ", __FILE__, ':',
					toString(__LINE__), " - ", e.what());
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
			UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn->
				prepareStatement("UPDATE submissions "
				"SET status='waiting', queued=? WHERE problem_id=?"));
			pstmt->setString(1, date("%Y-%m-%d %H:%M:%S"));
			pstmt->setString(2, r_path_->problem->problem_id);

			pstmt->executeUpdate();
			notifyJudgeServer();

		} catch (const std::exception& e) {
			error_log("Caught exception: ", __FILE__, ':', toString(__LINE__),
				" - ", e.what());
		}

		return sim_.redirect(sim_.req_->target.substr(0, arg_beg - 1));
	}

	// Download
	while (0 == compareTo(sim_.req_->target, arg_beg, '/', "download")) {
		arg_beg += 9;

		constexpr unsigned char empty_zip_file[] = {
			0x50, 0x4b, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		};

		// TODO: Simplify extension
		const char* _zip = ".zip";
		const char* _tgz = ".tar.gz";
		const char* extension;
		// Get extension
		if (0 == compareTo(sim_.req_->target, arg_beg, '/', "zip"))
			extension = _zip;
		else if (0 == compareTo(sim_.req_->target, arg_beg, '/', "tgz"))
			extension = _tgz;
		else
			break;

		// Create temporary file
		char tmp_file[] = "/tmp/sim-problem.XXXXXX";
		umask(077); // Only owner can access this temporary file
		int fd = mkstemp(tmp_file);
		if (fd == -1)
			throw std::runtime_error(concat("Error: mkstemp()", error(errno)));

		sclose(fd);
		FileRemover remover(tmp_file);
		vector<string> args;
		// zip
		if (extension == _zip) {
			append(args)("zip")("-rq")(tmp_file)(r_path_->problem->problem_id);
			if (putFileContents(tmp_file, (const char*)empty_zip_file,
					sizeof(empty_zip_file)) == -1)
				throw std::runtime_error(concat("Error: putFileContents()",
					error(errno)));
		// tar.gz
		} else // extension == tgz
			append(args)("tar")("czf")(tmp_file)(r_path_->problem->problem_id);

		// Compress package
		// TODO: add time limit
		// TODO: better error handling
		spawn_opts opts = { -1, STDERR_FILENO, STDERR_FILENO };
		int exit_code = spawn(args[0], args, &opts, "problems");
		if (exit_code != 0) {
			auto message = error_log("Error: ", args[0], " ");

			if (exit_code == -1)
				message("Failed to execute");
			else if (WIFSIGNALED(exit_code))
				message("killed by signal ", toString(WTERMSIG(exit_code)),
					" - ", strsignal(WTERMSIG(exit_code)));
			else if (WIFSTOPPED(exit_code))
				message("killed by signal ", toString(WSTOPSIG(exit_code)),
					" - ", strsignal(WSTOPSIG(exit_code)));
			else
				message("returned ", toString(WIFEXITED(exit_code) ?
					WEXITSTATUS(exit_code) : exit_code));

			return sim_.error500();
		}

		sim_.resp_.content_type = server::HttpResponse::FILE_TO_REMOVE;
		sim_.resp_.headers["Content-Disposition"] =
			concat("attachment; filename=", r_path_->problem->problem_id,
				extension);
		sim_.resp_.content = tmp_file;
		remover.cancel();
		return;
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

				if (putFileContents(
						concat("problems/", r_path_->problem->problem_id,
							"/config.conf"),
						pconfig.dump()) == -1)
					throw std::runtime_error("Failed to update problem " +
						r_path_->problem->problem_id + " config");

				// Update database
				UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn->
					prepareStatement("UPDATE rounds r, problems p "
						"SET r.name=?, p.name=?, p.tag=? WHERE r.id=? AND p.id=?"));
				pstmt->setString(1, round_name);
				pstmt->setString(2, name);
				pstmt->setString(3, tag);
				pstmt->setString(4, r_path_->round_id);
				pstmt->setString(5, r_path_->problem->problem_id);

				if (pstmt->executeUpdate() > 0) {
					// Update r_path_
					r_path_.reset(getRoundPath(r_path_->round_id));
					if (r_path_.isNull()) {
						error_log("Corrupt hierarchy of rounds (id: ",
							r_path_->round_id, ")");
						return sim_.error500();
					}
				}

			} catch (const std::exception& e) {
				error_log("Caught exception: ", __FILE__, ':',
					toString(__LINE__), " - ", e.what());
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
			"<div class=\"dropdown\" style=\"margin-left:5px\">"
				"<a class=\"btn-small dropdown-toggle\">"
					"Download package as<span class=\"caret\"></span></a>"
				"<ul>"
					"<a href=\"" << sim_.req_->target.substr(0, arg_beg - 1)
						<< "/download/zip\">.zip</a>"
					"<a href=\"" << sim_.req_->target.substr(0, arg_beg - 1)
						<< "/download/tgz\">.tar.gz</a>"
				"</ul>"
			"</div>\n"
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
					"<label>Memory limit [kB]</label>\n"
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
			UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn->
				prepareStatement("DELETE FROM submissions "
					"WHERE contest_round_id=?"));
			pstmt->setString(1, r_path_->round_id);
			pstmt->executeUpdate();

			// Delete from users_to_contests
			pstmt.reset(sim_.db_conn->prepareStatement(
				"DELETE FROM users_to_contests WHERE contest_id=?"));
			pstmt->setString(1, r_path_->round_id);
			pstmt->executeUpdate();

			// Delete rounds
			pstmt.reset(sim_.db_conn->prepareStatement("DELETE FROM rounds "
				"WHERE id=? OR parent=? OR grandparent=?"));
			pstmt->setString(1, r_path_->round_id);
			pstmt->setString(2, r_path_->round_id);
			pstmt->setString(3, r_path_->round_id);

			if (pstmt->executeUpdate() > 0)
				return sim_.redirect("/c");

		} catch (const std::exception& e) {
			error_log("Caught exception: ", __FILE__, ':', toString(__LINE__),
				" - ", e.what());
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
			UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn->
				prepareStatement("DELETE FROM submissions "
					"WHERE parent_round_id=?"));
			pstmt->setString(1, r_path_->round_id);
			pstmt->executeUpdate();

			// Delete rounds
			pstmt.reset(sim_.db_conn->prepareStatement(
				"DELETE FROM rounds WHERE id=? OR parent=?"));
			pstmt->setString(1, r_path_->round_id);
			pstmt->setString(2, r_path_->round_id);

			if (pstmt->executeUpdate() > 0)
				return sim_.redirect("/c/" + r_path_->contest->id);

		} catch (const std::exception& e) {
			error_log("Caught exception: ", __FILE__, ':', toString(__LINE__),
				" - ", e.what());
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
			UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn->
				prepareStatement("DELETE FROM submissions WHERE round_id=?"));
			pstmt->setString(1, r_path_->round_id);
			pstmt->executeUpdate();

			// Delete problem round
			pstmt.reset(sim_.db_conn->prepareStatement(
				"DELETE FROM rounds WHERE id=?"));
			pstmt->setString(1, r_path_->round_id);

			if (pstmt->executeUpdate() > 0)
				return sim_.redirect("/c/" + r_path_->round->id);

		} catch (const std::exception& e) {
			error_log("Caught exception: ", __FILE__, ':', toString(__LINE__),
				" - ", e.what());
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
			const RoundPath* problem_r_path = r_path_.get();

			if (r_path_->type != PROBLEM) {
				// Get parent rounds of problem round
				path.reset(getRoundPath(problem_round_id));
				if (path.isNull()) {
					error_log("Corrupt hierarchy of rounds (id: ",
						problem_round_id, ")");
					return sim_.error500();
				}

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
				UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn->
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
				UniquePtr<sql::Statement> stmt(sim_.db_conn->
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
				error_log("Caught exception: ", __FILE__, ':',
					toString(__LINE__), " - ", e.what());
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
			UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn->
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
			pstmt.reset(sim_.db_conn->prepareStatement(
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
				auto it = problems_table.find(res->getString(2));
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
			for (auto& subround : subrounds) {
				vector<Problem>& prob = problems_table[subround.id];

				for (auto& problem : prob)
					append(buffer) << "<option value=\"" << problem.id << "\">"
						<< htmlSpecialChars(problem.name) << " ("
						<< htmlSpecialChars(subround.name) << ")</option>\n";
			}

		// Admin -> All problems
		// Normal -> if round has begun and has not ended
		} else if (r_path_->type == ROUND && (admin_view || (
				r_path_->round->begins <= current_date && // "" <= everything
				(r_path_->round->ends.empty() ||
					current_date < r_path_->round->ends)))) {
			// Select problems
			UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn->
				prepareStatement("SELECT id, name FROM rounds "
					"WHERE parent=? ORDER BY item"));
			pstmt->setString(1, r_path_->round->id);

			// List problems
			UniquePtr<sql::ResultSet> res(pstmt->executeQuery());
			while (res->next())
				append(buffer) << "<option value=\"" << res->getString(1)
					<< "\">" << htmlSpecialChars(StringView(res->getString(2)))
					<< " (" << htmlSpecialChars(r_path_->round->name)
					<< ")</option>\n";

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
		error_log("Caught exception: ", __FILE__, ':', toString(__LINE__),
			" - ", e.what());
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
		enum class Query {
			DELETE, REJUDGE, DOWNLOAD, VIEW_SOURCE, NONE
		} query = Query::NONE;

		if (0 == compareTo(sim_.req_->target, arg_beg, '/', "delete"))
			query = Query::DELETE;
		else if (0 == compareTo(sim_.req_->target, arg_beg, '/', "rejudge"))
			query = Query::REJUDGE;
		else if (0 == compareTo(sim_.req_->target, arg_beg, '/', "download"))
			query = Query::DOWNLOAD;
		else if (0 == compareTo(sim_.req_->target, arg_beg, '/', "source"))
			query = Query::VIEW_SOURCE;

		// Get submission
		const char* columns = (query != Query::NONE ? "" : ", submit_time, "
			"status, score, name, tag, first_name, last_name, initial_report, "
			"final_report");
		UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn->
			prepareStatement(concat("SELECT user_id, round_id", columns, " "
				"FROM submissions s, problems p, users u "
				"WHERE s.id=? AND s.problem_id=p.id AND u.id=user_id")));
		pstmt->setString(1, submission_id);

		UniquePtr<sql::ResultSet> res(pstmt->executeQuery());
		if (!res->next())
			return sim_.error404();

		string submission_user_id = res->getString(1);
		string round_id = res->getString(2);

		// Get parent rounds
		r_path_.reset(getRoundPath(round_id));
		if (r_path_.isNull()) {
			error_log("Corrupt hierarchy of rounds (id: ", round_id, ")");
			return sim_.error500();
		}

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
		if (query == Query::DELETE) {
			if (!admin_view)
				return sim_.error403();

			FormValidator fv(sim_.req_->form_data);
			while (sim_.req_->method == server::HttpRequest::POST
					&& fv.exist("delete"))
				try {
					// Delete submission
					pstmt.reset(sim_.db_conn->prepareStatement(
						"DELETE FROM submissions WHERE id=?"));
					pstmt->setString(1, submission_id);

					if (pstmt->executeUpdate() == 0) {
						fv.addError("Deletion failed");
						break;
					}

					// Restore `final` status
					// TODO: try to simplify following UPDATE (see judge
					// machine submission update)
					pstmt.reset(sim_.db_conn->prepareStatement(
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
					error_log("Caught exception: ", __FILE__, ':',
						toString(__LINE__), " - ", e.what());
				}

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
		if (query == Query::REJUDGE) {
			if (!admin_view)
				return sim_.error403();

			pstmt.reset(sim_.db_conn->prepareStatement("UPDATE submissions "
				"SET status='waiting', queued=? WHERE id=?"));
			pstmt->setString(1, date("%Y-%m-%d %H:%M:%S"));
			pstmt->setString(2, submission_id);

			pstmt->executeUpdate();
			notifyJudgeServer();

			return sim_.redirect(sim_.req_->target.substr(0, arg_beg - 1));
		}

		// Download solution
		if (query == Query::DOWNLOAD) {
			sim_.resp_.headers["Content-type"] = "application/text";
			sim_.resp_.headers["Content-Disposition"] =
				concat("attachment; filename=", submission_id, ".cpp");

			sim_.resp_.content = concat("solutions/", submission_id, ".cpp");
			sim_.resp_.content_type = server::HttpResponse::FILE;

			return;
		}

		TemplateWithMenu templ(*this, "Submission " + submission_id);
		templ.printRoundPath(*r_path_, "");

		// View source
		if (query == Query::VIEW_SOURCE) {
			vector<string> args;
			append(args)("./CTH")("solutions/" + submission_id + ".cpp");

			spawn_opts sopt = {
				-1,
				getUnlinkedTmpFile(),
				-1
			};

			if (sopt.new_stdout_fd < 0)
				return sim_.error500();

			Closer closer(sopt.new_stdout_fd);

			// Run CTH
			int exit_code = spawn(args[0], args, &sopt);
			if (exit_code != 0) {
				auto message = error_log("Error: ", args[0], " ");

				if (exit_code == -1)
					message("Failed to execute");
				else if (WIFSIGNALED(exit_code))
					message("killed by signal ", toString(WTERMSIG(exit_code)),
						" - ", strsignal(WTERMSIG(exit_code)));
				else if (WIFSTOPPED(exit_code))
					message("killed by signal ", toString(WSTOPSIG(exit_code)),
						" - ", strsignal(WSTOPSIG(exit_code)));
				else
					message("returned ", toString(WIFEXITED(exit_code) ?
						WEXITSTATUS(exit_code) : exit_code));

				return sim_.error500();
			}

			// Print source code
			lseek(sopt.new_stdout_fd, 0, SEEK_SET);
			templ << getFileContents(sopt.new_stdout_fd);

			return;
		}

		string submit_time = res->getString(3);
		string submission_status = res->getString(4);
		string score = res->getString(5);
		string problem_name = res->getString(6);
		string problem_tag = res->getString(7);
		string user_name = concat(res->getString(8), ' ', res->getString(9));

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
					"<tr>";

		if (admin_view)
			templ << "<th style=\"min-width:120px\">User</th>";

		templ << "<th style=\"min-width:120px\">Problem</th>"
						"<th style=\"min-width:150px\">Submission time</th>"
						"<th style=\"min-width:150px\">Status</th>"
						"<th style=\"min-width:90px\">Score</th>"
					"</tr>\n"
				"</thead>\n"
				"<tbody>\n"
					"<tr>";

		if (admin_view)
			templ << "<td><a href=\"/u/" << submission_user_id << "\">"
				<< user_name << "</a></td>";

		templ << "<td>" << htmlSpecialChars(
			concat(problem_name, " (", problem_tag, ')')) << "</td>"
						"<td>" << htmlSpecialChars(submit_time) << "</td>"
						"<td";

		if (submission_status == "ok")
			templ << " class=\"ok\"";
		else if (submission_status == "error")
			templ << " class=\"wa\"";
		else if (submission_status == "c_error")
			templ << " class=\"tl-rte\"";
		else if (submission_status == "judge_error")
			templ << " class=\"judge-error\"";

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

		// Print judge report
		templ << "<div class=\"results\">";
		if (submission_status == "c_error") {
			templ << "<h2>Compilation failed</h2>"
				"<pre class=\"compile-errors\">"
				<< res->getString(10)
				<< "</pre>";

		} else {
			if (admin_view || r_path_->round->full_results.empty() ||
					r_path_->round->full_results <= date("%Y-%m-%d %H:%M:%S")) {
				string final_report = res->getString(11);
				if (final_report.size())
					templ << "<h2>Final testing report</h2>"
						<< final_report;
			}

			string initial_report = res->getString(10);
			if (initial_report.size())
				templ << "<h2>Initial testing report</h2>"
					<< initial_report;
		}
		templ << "</div>";

	} catch (const std::exception& e) {
		error_log("Caught exception: ", __FILE__, ':', toString(__LINE__),
			" - ", e.what());
	}
}

void Sim::Contest::submissions(bool admin_view) {
	if (sim_.session->open() != Sim::Session::OK)
		return sim_.redirect("/login" + sim_.req_->target);

	TemplateWithMenu templ(*this, "Submissions");
	templ << "<h1>Submissions</h1>";
	templ.printRoundPath(*r_path_, "submissions", !admin_view);

	templ << "<h3>Submission queue size: ";
	try {
		UniquePtr<sql::Statement> stmt(sim_.db_conn->createStatement());
		UniquePtr<sql::ResultSet> res(stmt->executeQuery(
			"SELECT COUNT(*) FROM submissions WHERE status='waiting';"));
		if (res->next())
			templ << res->getString(1);

	} catch (const std::exception& e) {
		error_log("Caught exception: ", __FILE__, ':', toString(__LINE__),
			" - ", e.what());
	}

	templ <<  "</h3>";

	try {
		string param_column = (r_path_->type == CONTEST ? "contest_round_id"
			: (r_path_->type == ROUND ? "parent_round_id" : "round_id"));

		UniquePtr<sql::PreparedStatement> pstmt(sim_.db_conn->
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
				else if (status == "c_error")
					ret += " class=\"tl-rte\">";
				else if (status == "judge_error")
					ret += " class=\"judge-error\">";
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
					<< htmlSpecialChars(StringView(res->getString(11)))
					<< "</a></td>";
			// Rest
			templ << "<td><a href=\"/s/" << res->getString(1) << "\">"
					<< res->getString(2) << "</a></td>"
					"<td><a href=\"/c/" << res->getString(3) << "\">"
						<< htmlSpecialChars(StringView(res->getString(4)))
						<< "</a> -> <a href=\"/c/" << res->getString(5) << "\">"
						<< htmlSpecialChars(StringView(res->getString(6)))
						<< "</a></td>"
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
		error_log("Caught exception: ", __FILE__, ':', toString(__LINE__),
			" - ", e.what());
	}
}

template<class T>
static typename T::const_iterator findWithId(const T& x, const string& id)
		noexcept {
	typename T::const_iterator beg = x.begin(), end = x.end(), mid;
	while (beg != end) {
		mid = beg + ((end - beg) >> 1);
		if (mid->get().id < id)
			beg = ++mid;
		else
			end = mid;
	}
	return (beg != x.end() && beg->get().id == id ? beg : x.end());
}

void Sim::Contest::ranking(bool admin_view) {
	if (!admin_view && !r_path_->contest->show_ranking)
		return sim_.error403();

	TemplateWithMenu templ(*this, "Ranking");
	templ << "<h1>Ranking</h1>";
	templ.printRoundPath(*r_path_, "ranking", !admin_view);

	struct RankingProblem {
		unsigned long long id;
		string tag;
	};

	struct RankingRound {
		string id, name, item;
		vector<RankingProblem> problems;

		bool operator<(const RankingRound& x) const {
			return StrNumCompare()(item, x.item);
		}
	};

	struct RankingField {
		string submission_id, round_id, score;
	};

	struct RankingRow {
		string user_id, name;
		long long score;
		vector<RankingField> fields;
	};

	struct cmp {
		bool operator()(const RankingRound& a, const RankingRound& b) const {
			return a.id < b.id;
		}

		bool operator()(const RankingRow& a, const RankingRow& b) const {
			return a.score > b.score;
		}
	};

	try {
		UniquePtr<sql::PreparedStatement> pstmt;
		UniquePtr<sql::ResultSet> res;
		string current_time = date("%Y-%m-%d %H:%M:%S");

		// Select rounds
		const char* column = (r_path_->type == CONTEST ? "parent" : "id");
		pstmt.reset(sim_.db_conn->prepareStatement(admin_view ?
			concat("SELECT id, name, item FROM rounds WHERE ", column, "=?")
			: concat("SELECT id, name, item FROM rounds WHERE ", column, "=? "
				"AND (full_results IS NULL OR full_results<=?)")));

		pstmt->setString(1, r_path_->type == CONTEST
			? r_path_->round_id
			: r_path_->round->id);
		if (!admin_view)
			pstmt->setString(2, current_time);
		res.reset(pstmt->executeQuery());

		vector<RankingRound> rounds;
		rounds.reserve(res->rowsCount()); // Need for pointers validity
		vector<std::reference_wrapper<RankingRound>> rounds_by_id;
		while (res->next()) {
			rounds.push_back((RankingRound){
				res->getString(1),
				res->getString(2),
				res->getString(3),
				vector<RankingProblem>()
			});
			rounds_by_id.emplace_back(rounds.back());
		}
		if (rounds.empty()) {
			templ << "<p>There is no one in the ranking yet...</p>";
			return;
		}

		sort(rounds_by_id.begin(), rounds_by_id.end(), cmp());

		// Select problems
		column = (r_path_->type == CONTEST ? "grandparent" :
			(r_path_->type == ROUND ? "parent" : "id"));
		pstmt.reset(sim_.db_conn->prepareStatement(admin_view ?
			concat("SELECT r.id, tag, parent FROM rounds r, problems p "
				"WHERE r.", column, "=? AND problem_id=p.id ORDER BY item")
			: concat("SELECT r.id, tag, r.parent "
				"FROM rounds r, rounds r1, problems p "
				"WHERE r.", column, "=? AND r.problem_id=p.id "
					"AND r.parent=r1.id "
					"AND (r1.full_results IS NULL OR r1.full_results<=?)")));
		pstmt->setString(1, r_path_->round_id);
		if (!admin_view)
			pstmt->setString(2, current_time);
		res.reset(pstmt->executeQuery());

		// Add problems to rounds
		while (res->next()) {
			auto it = findWithId(rounds_by_id, res->getString(3));
			if (it == rounds_by_id.end())
				continue; // Ignore invalid rounds hierarchy

			it->get().problems.push_back((RankingProblem){
				res->getUInt64(1),
				res->getString(2)
			});
		}

		rounds_by_id.clear(); // Free unused memory
		sort(rounds.begin(), rounds.end());

		// Select submissions
		column = (r_path_->type == CONTEST ? "contest_round_id" :
			(r_path_->type == ROUND ? "parent_round_id" : "round_id"));
		pstmt.reset(sim_.db_conn->prepareStatement(admin_view ?
			concat("SELECT s.id, user_id, u.first_name, u.last_name, round_id, "
					"score "
				"FROM submissions s, users u "
				"WHERE s.", column, "=? AND final=1 AND user_id=u.id "
				"ORDER BY user_id")
			: concat("SELECT s.id, user_id, u.first_name, u.last_name, "
					"round_id, score "
				"FROM submissions s, users u, rounds r "
				"WHERE s.", column, "=? AND final=1 AND user_id=u.id "
					"AND r.id=parent_round_id "
					"AND (full_results IS NULL OR full_results<=?) "
				"ORDER BY user_id")));
		pstmt->setString(1, r_path_->round_id);
		if (!admin_view)
			pstmt->setString(2, current_time);
		res.reset(pstmt->executeQuery());

		// Construct rows
		vector<RankingRow> rows;
		string last_user_id;
		while (res->next()) {
			// Next user
			if (last_user_id != res->getString(2)) {
				rows.push_back((RankingRow){
					res->getString(2),
					res->getString(3) + " " + res->getString(4),
					0,
					vector<RankingField>()
				});
			}
			last_user_id = rows.back().user_id;

			rows.back().score += res->getInt(6);
			rows.back().fields.push_back((RankingField){
				res->getString(1),
				res->getString(5),
				res->getString(6)
			});
		}

		// Sort rows
		vector<std::reference_wrapper<RankingRow> > sorted_rows;
		sorted_rows.reserve(rows.size());
		for (size_t i = 0; i < rows.size(); ++i)
			sorted_rows.emplace_back(rows[i]);
		sort(sorted_rows.begin(), sorted_rows.end(), cmp());

		// Print rows
		if (rows.empty()) {
			templ << "<p>There is no one in the ranking yet...</p>";
			return;
		}
		// Make problem index
		size_t idx = 0;
		vector<pair<size_t, size_t> > index_of; // (problem_id, index)
		for (auto& i : rounds)
			for (auto& j : i.problems)
				index_of.emplace_back(j.id, idx++);
		sort(index_of.begin(), index_of.end());

		// Table head
		templ << "<table class=\"table ranking\">\n"
				"<thead>\n"
					"<tr>\n"
						"<th rowspan=\"2\">#</th>\n"
						"<th rowspan=\"2\">User</th>\n";
		// Rounds
		for (auto& i : rounds) {
			if (i.problems.empty())
				continue;

			templ << "<th";
			if (i.problems.size() > 1)
				templ << " colspan=\"" << toString(i.problems.size())
					<< "\"";
			templ << "><a href=\"/c/" << i.id
				<< (admin_view ? "/ranking\">" : "/n/ranking\">")
				<< htmlSpecialChars(i.name) << "</a></th>\n";
		}
		// Problems
		templ << "<th rowspan=\"2\">Sum</th>\n"
			"</tr>\n"
			"<tr>\n";
		for (auto& i : rounds)
			for (auto& j : i.problems)
				templ << "<th><a href=\"/c/" << toString(j.id)
					<< (admin_view ? "/ranking\">" : "/n/ranking\">")
					<< htmlSpecialChars(j.tag) << "</a></th>";
		templ << "</tr>\n"
			"</thead>\n"
			"<tbody>\n";
		// Rows
		assert(sorted_rows.size());
		size_t place = 1; // User place
		long long last_user_score = sorted_rows.front().get().score;
		vector<RankingField*> row_points(idx); // idx is now number of problems
		for (size_t i = 0, end = sorted_rows.size(); i < end; ++i) {
			RankingRow& row = sorted_rows[i];
			// Place
			if (last_user_score != row.score)
				place = i + 1;
			last_user_score = row.score;
			templ << "<tr>\n"
					"<td>" << toString(place) << "</td>\n";
			// Name
			if (admin_view)
				templ << "<td><a href=\"/u/" << row.user_id << "\">"
					<< htmlSpecialChars(row.name) << "</a></td>\n";
			else
				templ << "<td>" << htmlSpecialChars(row.name) << "</td>\n";

			// Fields
			fill(row_points.begin(), row_points.end(), nullptr);
			for (auto& j : row.fields) {
				vector<pair<size_t, size_t> >::const_iterator it =
					binaryFindBy(index_of, &pair<size_t, size_t>::first,
						strtoull(j.round_id));
				if (it == index_of.end())
					throw std::runtime_error("Failed to get index of problem");

				row_points[it->second] = &j;
			}
			for (auto& j : row_points) {
				if (j == nullptr)
					templ << "<td></td>\n";
				else if (admin_view || (sim_.session->state() == Session::OK &&
						row.user_id == sim_.session->user_id))
					templ << "<td><a href=\"/s/" << j->submission_id << "\">"
						<< j->score << "</a></td>\n";
				else
					templ << "<td>" << j->score << "</td>\n";
			}

			templ << "<td>" << toString(row.score) << "</td>"
				"</tr>\n";
		}
		templ << "</tbody>\n"
				"</thead>\n"
			"</table>\n";

	} catch (const std::exception& e) {
		error_log("Caught exception: ", __FILE__, ':', toString(__LINE__),
			" - ", e.what());
	}
}

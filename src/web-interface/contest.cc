#include "form_validator.h"
#include "contest.h"

#include <sim/utilities.h>
#include <simlib/config_file.h>
#include <simlib/debug.h>
#include <simlib/filesystem.h>
#include <simlib/process.h>
#include <simlib/random.h>
#include <simlib/sim/conver.h>
#include <simlib/spawner.h>
#include <simlib/time.h>

using std::array;
using std::pair;
using std::string;
using std::vector;

void Contest::handle() {
	// Select contest
	StringView next_arg = url_args.extractNextArg();
	if (next_arg.empty()) {
		try {
			// Get available contests
			DB::Statement stmt;
			do {
				// Not logged in
				if (!Session::open()) {
					stmt = db_conn.prepare("SELECT id, name FROM rounds "
						"WHERE parent IS NULL AND is_public IS TRUE "
						"ORDER BY id");
					break;
				}

				// Sim root
				if (UNLIKELY(Session::user_id == SIM_ROOT_UID)) {
					stmt = db_conn.prepare("SELECT id, name FROM rounds "
						"WHERE parent is NULL ORDER BY id");
					break;
				}

				// Normal user + Moderator
				if (Session::user_type == UTYPE_NORMAL) {
					stmt = db_conn.prepare("(SELECT id, name FROM rounds "
							"WHERE parent IS NULL AND "
								"(is_public IS TRUE OR owner=?))"
						" UNION "
						"(SELECT id, name FROM rounds, contests_users "
							"WHERE user_id=? AND contest_id=id) ORDER BY id");
					stmt.setString(1, Session::user_id);
					stmt.setString(2, Session::user_id);
					break;
				}

				// Admin + Teacher + Moderator
				stmt = db_conn.prepare(
					"(SELECT r.id, r.name FROM rounds r, users u "
						"WHERE parent IS NULL AND owner=u.id AND "
							"(is_public IS TRUE OR owner=? OR u.type>?))"
					" UNION "
					"(SELECT id, name FROM rounds, contests_users "
						"WHERE user_id=? AND contest_id=id) ORDER BY id");
				stmt.setString(1, Session::user_id);
				stmt.setUInt(2, Session::user_type);
				stmt.setString(3, Session::user_id);
			} while (0);

			baseTemplate("Select contest");

			/* List them */
			append("<div class=\"contests-list\">");

			// Add contest button (admins and teachers only)
			if (Session::isOpen() && Session::user_type < UTYPE_NORMAL)
				append("<a class=\"btn\" href=\"/c/add\">Add contest</a>");

			DB::Result res = stmt.executeQuery();
			if (res.next()) {
				do {
					append("<a href=\"/c/", htmlEscape(res[1]), "\">",
						htmlEscape(res[2]), "</a>");
				} while (res.next());
			} else {
				append("<p class=\"pos-center\""
					" style=\"margin:0;padding:5px;border-top:1px solid #ccc\">"
					"There are no contests to show...</p>");
			}

			append("</div>");
			return;

		} catch (const std::exception& e) {
			ERRLOG_CATCH(e);
			return error500();
		}
	}

	// Add contest
	if (next_arg == "add")
		return addContest();

	/* Other pages which need round id */
	// Extract round id
	string round_id;
	if (strToNum(round_id, next_arg) <= 0)
		return error404();

	next_arg = url_args.extractNextArg();

	// Get parent rounds
	rpath.reset(getRoundPath(round_id));
	if (!rpath)
		return; // getRoundPath has already set an error

	// Check if user forces observer view
	bool admin_view = rpath->admin_access;
	if (next_arg == "n") {
		admin_view = false;
		next_arg = url_args.extractNextArg();
	}

	// Problem statement
	if (rpath->type == PROBLEM && next_arg == "statement")
		return Problemset::problemStatement(rpath->problem->problem_id);

	// Add
	if (next_arg == "add") {
		if (rpath->type == CONTEST)
			return addRound();

		if (rpath->type == ROUND)
			return addProblem();

		return error404();
	}

	// Edit
	if (next_arg == "edit") {
		if (rpath->type == CONTEST)
			return editContest();

		if (rpath->type == ROUND)
			return editRound();

		return editProblem();
	}

	// Delete
	if (next_arg == "delete") {
		if (rpath->type == CONTEST)
			return deleteContest();

		if (rpath->type == ROUND)
			return deleteRound();

		return deleteProblem();
	}

	// Users
	if (next_arg == "users")
		return users();

	// Problems
	if (next_arg == "problems")
		return listProblems(admin_view);

	// Submit
	if (next_arg == "submit")
		return submit(admin_view);

	// Submissions
	if (next_arg == "submissions")
		return submissions(admin_view);

	// Ranking
	if (next_arg == "ranking")
		return ranking(admin_view);

	// Files
	if (next_arg == "files")
		return files(admin_view);

	// Contest dashboard
	contestTemplate("Contest dashboard");
	append("<div class=\"round-info\">");

	if (rpath->type == CONTEST) {
		append("<h1>", htmlEscape(rpath->contest->name), "</h1>");
		if (admin_view)
			append("<div>"
					"<a class=\"btn-small\" href=\"/c/", round_id, "/add\">"
						"Add round</a>"
					"<a class=\"btn-small blue\" href=\"/c/", round_id,
						"/edit\">Edit contest</a>"
				"</div>");

	} else if (rpath->type == ROUND) {
		append("<h1>", htmlEscape(rpath->round->name), "</h1>");
		if (admin_view)
			append("<div>"
					"<a class=\"btn-small\" href=\"/c/", round_id, "/add\">"
						"Add problem</a>"
					"<a class=\"btn-small blue\" href=\"/c/", round_id,
						"/edit\">Edit round</a>"
				"</div>");

	} else { // rpath->type == PROBLEM
		append("<h1>", htmlEscape(rpath->problem->name), "</h1>");
		if (admin_view)
			append("<div>"
				"<a class=\"btn-small\" href=\"/c/", rpath->round->id,
					"/add\">Add problem</a>"
				"<a class=\"btn-small blue\" href=\"/c/", round_id,
					"/edit\">Edit problem</a>"
				"</div>");
	}

	append("</div>");
	printRoundPath();

	printRoundView(false, admin_view);

	if (rpath->type == PROBLEM)
		append("<a class=\"btn\" href=\"/c/", round_id, "/statement\" "
			"style=\"margin:5px auto;display:table\">View statement</a>");
}

void Contest::addContest() {
	if (!Session::open() || Session::user_type > UTYPE_TEACHER)
		return error403();

	FormValidator fv(req->form_data);
	string name;
	bool is_public = false, show_ranking = false;

	if (req->method == server::HttpRequest::POST) {
		if (fv.get("csrf_token") != Session::csrf_token)
			return error403();

		// Validate all fields
		fv.validateNotBlank(name, "name", "Contest name", ROUND_NAME_MAX_LEN);
		is_public = fv.exist("public");
		// Only admins can create public contests
		if (is_public && Session::user_type > UTYPE_ADMIN) {
			is_public = false;
			fv.addError("Only admins can create public contests");
		}
		show_ranking = fv.exist("show-ranking");

		// If all fields are ok
		if (fv.noErrors())
			try {
				DB::Statement stmt = db_conn.prepare("INSERT rounds"
						"(is_public, name, owner, item, show_ranking) "
					"SELECT ?, ?, ?, COALESCE(MAX(item)+1, 1), ? FROM rounds "
						"WHERE parent IS NULL");
				stmt.setBool(1, is_public);
				stmt.setString(2, name);
				stmt.setString(3, Session::user_id);
				stmt.setBool(4, show_ranking);

				if (stmt.executeUpdate() != 1)
					THROW("Failed to insert round");

				DB::Result res = db_conn.executeQuery(
					"SELECT LAST_INSERT_ID()");

				if (res.next())
					return redirect("/c/" + res[1]);

				return redirect("/c");

			} catch (const std::exception& e) {
				ERRLOG_CATCH(e);
				fv.addError("Internal server error");
			}
	}

	baseTemplate("Add contest", "body{margin-left:30px}");
	append(fv.errors(), "<div class=\"form-container\">" // TODO: center
			"<h1>Add contest</h1>"
			"<form method=\"post\">"
				// Name
				"<div class=\"field-group\">"
					"<label>Contest name</label>"
					"<input type=\"text\" name=\"name\" value=\"",
						htmlEscape(name), "\" size=\"24\" "
						"maxlength=\"", toStr(ROUND_NAME_MAX_LEN), "\" "
						"required>"
				"</div>"
				// Public
				"<div class=\"field-group\">"
					"<label>Public</label>"
					"<input type=\"checkbox\" name=\"public\"",
						(is_public ? " checked" : ""),
						(Session::user_type > UTYPE_ADMIN ? " disabled" : ""),
						">"
				"</div>"
				// Show ranking
				"<div class=\"field-group\">"
					"<label>Show ranking</label>"
					"<input type=\"checkbox\" name=\"show-ranking\"",
						(show_ranking ? " checked" : ""), ">"
				"</div>"
				"<input class=\"btn blue\" type=\"submit\" value=\"Add\">"
			"</form>"
		"</div>");
}

void Contest::addRound() {
	if (!rpath->admin_access)
		return error403();

	FormValidator fv(req->form_data);
	string name;
	bool is_visible = false;
	string begins, full_results, ends;

	if (req->method == server::HttpRequest::POST) {
		if (fv.get("csrf_token") != Session::csrf_token)
			return error403();

		// Validate all fields
		fv.validateNotBlank(name, "name", "Round name", ROUND_NAME_MAX_LEN);
		is_visible = fv.exist("visible");
		fv.validate(begins, "begins", "Begins", isDatetime,
			"Begins: invalid value");
		fv.validate(ends, "ends", "Ends", isDatetime, "Ends: invalid value");
		fv.validate(full_results, "full_results", "Ends", isDatetime,
			"Full_results: invalid value");

		// If all fields are ok
		if (fv.noErrors())
			try {
				DB::Statement stmt = db_conn.prepare(
					"INSERT rounds (parent, name, owner, item, "
						"visible, begins, ends, full_results) "
					"SELECT ?, ?, 0, COALESCE(MAX(item)+1, 1), ?, ?, ?, ? "
						"FROM rounds "
						"WHERE parent=?");
				stmt.setString(1, rpath->round_id);
				stmt.setString(2, name);
				stmt.setBool(3, is_visible);

				// Begins
				if (begins.empty())
					stmt.setNull(4);
				else
					stmt.setString(4, begins);

				// ends
				if (ends.empty())
					stmt.setNull(5);
				else
					stmt.setString(5, ends);

				// Full_results
				if (full_results.empty())
					stmt.setNull(6);
				else
					stmt.setString(6, full_results);

				stmt.setString(7, rpath->round_id);

				if (stmt.executeUpdate() != 1)
					THROW("Failed to insert round");

				DB::Result res = db_conn.executeQuery(
					"SELECT LAST_INSERT_ID()");

				if (res.next())
					return redirect("/c/" + res[1]);

				return redirect("/c/" + rpath->round_id);

			} catch (const std::exception& e) {
				ERRLOG_CATCH(e);
				fv.addError("Internal server error");
			}
	}

	contestTemplate("Add round");
	printRoundPath();
	append(fv.errors(), "<div class=\"form-container\">"
		"<h1>Add round</h1>"
		"<form method=\"post\">"
			// Name
			"<div class=\"field-group\">"
				"<label>Round name</label>"
				"<input type=\"text\" name=\"name\" value=\"",
					htmlEscape(name), "\" size=\"24\" "
					"maxlength=\"", toStr(ROUND_NAME_MAX_LEN), "\" required>"
			"</div>"
			// Visible
			"<div class=\"field-group\">"
				"<label>Visible</label>"
				"<input type=\"checkbox\" name=\"visible\"",
					(is_visible ? " checked" : ""), ">"
			"</div>"
			// Begins
			"<div class=\"field-group\">"
				"<label>Begins</label>"
				"<input type=\"text\" name=\"begins\""
					"placeholder=\"yyyy-mm-dd HH:MM:SS\" value=\"",
					htmlEscape(begins), "\" size=\"19\" "
					"maxlength=\"19\">"
					"<span>UTC</span>"
			"</div>"
			// Ends
			"<div class=\"field-group\">"
				"<label>Ends</label>"
				"<input type=\"text\" name=\"ends\""
					"placeholder=\"yyyy-mm-dd HH:MM:SS\" value=\"",
					htmlEscape(ends), "\" size=\"19\" "
					"maxlength=\"19\">"
					"<span>UTC</span>"
			"</div>"
			// Full_results
			"<div class=\"field-group\">"
				"<label>Full_results</label>"
				"<input type=\"text\" name=\"full_results\""
					"placeholder=\"yyyy-mm-dd HH:MM:SS\" value=\"",
					htmlEscape(full_results), "\" size=\"19\" "
					"maxlength=\"19\">"
					"<span>UTC</span>"
			"</div>"
			"<input class=\"btn blue\" type=\"submit\" value=\"Add\">"
		"</form>"
	"</div>");
}

void Contest::addProblem() {
	if (!rpath->admin_access)
		return error403();

	FormValidator fv(req->form_data);
	string name, memory_limit, user_package_file, time_limit;
	bool force_auto_limit = true, ignore_simfile = false;

	if (req->method == server::HttpRequest::POST) {
		if (fv.get("csrf_token") != Session::csrf_token)
			return error403();

		string package_file;

		// Validate all fields
		fv.validate(name, "name", "Problem name", PROBLEM_NAME_MAX_LEN);

		fv.validate(memory_limit, "memory-limit", "Memory limit",
			isDigitNotGreaterThan<(std::numeric_limits<uint64_t>::max() >> 20)>,
			"Memory limit: invalid value");

		fv.validate<bool(const StringView&)>(time_limit, "time-limit",
			"Time limit", isReal, "Time limit: invalid value");// TODO: add length limit
		// Time limit in usec
		uint64_t tl = round(strtod(time_limit.c_str(), nullptr) * 1'000'000);
		if (time_limit.size() && tl < 400000)
			fv.addError("Global time limit cannot be lower than 0.4 s");

		force_auto_limit = fv.exist("force-auto-limit");

		ignore_simfile = fv.exist("ignore-simfile");

		fv.validateNotBlank(user_package_file, "package", "Package");

		fv.validateFilePathNotEmpty(package_file, "package", "Package");

		// If all fields are OK
		if (fv.noErrors())
			try {
				// Rename package file that it will end with original extension
				string new_package_file = concat(package_file, '.',
					(hasSuffix(user_package_file, ".tar.gz") ? "tar.gz"
						: getExtension(user_package_file)));
				if (link(package_file.c_str(), new_package_file.c_str()))
					THROW("Error: link('", package_file, "', '",
						new_package_file, "')", error(errno));

				FileRemover file_rm(new_package_file);

				// Construct Simfile
				sim::Conver conver;
				sim::Simfile sf;
				try {
					conver.setVerbosity(true);
					conver.extractPackage(new_package_file);

					// Set Conver options
					sim::Conver::Options opts;
					opts.name = name;
					opts.force_auto_time_limits_setting = force_auto_limit;
					opts.ignore_simfile = ignore_simfile;
					opts.compilation_time_limit = SOLUTION_COMPILATION_TIME_LIMIT;
					opts.compilation_errors_max_length =
						COMPILATION_ERRORS_MAX_LENGTH;
					opts.proot_path = PROOT_PATH;

					if (memory_limit.size())
						opts.memory_limit = strtoull(memory_limit);

					if (time_limit.size())
						opts.global_time_limit = tl;

					sf = conver.constructFullSimfile(opts);
				} catch (const std::exception& e) {
					fv.addError(concat("Conver failed: ", e.what()));
					goto form;
				}

				// Put the Simfile in the package
				BLOCK_SIGNALS(putFileContents(
					conver.unpackedPackagePath() + "Simfile", sf.dump()));

				// 'Transaction' begin
				// Insert problem
				DB::Statement stmt = db_conn.prepare(
					"INSERT problems (name, label, owner, added) "
						"VALUES('', '', 0, ?)");
				stmt.setString(1, date("%Y-%m-%d %H:%M:%S"));
				if (1 != stmt.executeUpdate())
					THROW("Failed to insert problem");

				// Get problem_id
				DB::Result res = db_conn.executeQuery(
					"SELECT LAST_INSERT_ID()");
				if (!res.next())
					THROW("Failed to get LAST_INSERT_ID()");

				string problem_id = res[1];

				// Insert round (parent 0 - round will not be visible in case of
				// error)
				if (1 != db_conn.executeUpdate(
					"INSERT rounds (parent, name, owner, item) "
					"VALUES(0, '', 0, 0)"))
				{
					THROW("Failed to insert round");
				}

				// Get round_id
				res = db_conn.executeQuery("SELECT LAST_INSERT_ID()");
				if (!res.next())
					THROW("Failed to get LAST_INSERT_ID()");

				string round_id = res[1];

				// Move package folder to problems/
				string package_dir = concat("problems/", problem_id);
				if (::move(conver.unpackedPackagePath(), package_dir, false))
					THROW("Error: move('", conver.unpackedPackagePath(), "', '",
						package_dir, "')", error(errno));

				// Commit - update problem and round
				stmt = db_conn.prepare(
					"UPDATE problems p, rounds r,"
							"(SELECT COALESCE(MAX(item)+1, 1) x FROM rounds "
								"WHERE parent=?) t "
						"SET p.name=?, p.label=?, p.owner=?, "
							"parent=?, grandparent=?, r.name=?, item=t.x, "
							"problem_id=? "
						"WHERE p.id=? AND r.id=?");

				stmt.setString(1, rpath->round->id);
				stmt.setString(2, sf.name);
				stmt.setString(3, sf.label);
				stmt.setString(4, Session::user_id);
				stmt.setString(5, rpath->round->id);
				stmt.setString(6, rpath->contest->id);
				stmt.setString(7, sf.name);
				stmt.setString(8, problem_id);
				stmt.setString(9, problem_id);
				stmt.setString(10, round_id);

				if (2 != stmt.executeUpdate())
					THROW("Failed to update");

				return redirect(concat("/c/", round_id, "/edit"));

			} catch (const std::exception& e) {
				ERRLOG_CATCH(e);
				fv.addError("Internal server error");
			}
	}

 form:
	contestTemplate("Add problem");
	printRoundPath();
	append(fv.errors(), "<div class=\"form-container\">"
			"<h1>Add problem</h1>"
			"<form method=\"post\" enctype=\"multipart/form-data\">"
				// Name
				"<div class=\"field-group\">"
					"<label>Problem name</label>"
					"<input type=\"text\" name=\"name\" value=\"",
						htmlEscape(name), "\" size=\"24\""
					"maxlength=\"", toStr(PROBLEM_NAME_MAX_LEN), "\" "
					"placeholder=\"Detect from Simfile\">"
				"</div>"
				// Memory limit
				"<div class=\"field-group\">"
					"<label>Memory limit [MB]</label>"
					"<input type=\"text\" name=\"memory-limit\" value=\"",
						htmlEscape(memory_limit), "\" size=\"24\" "
					"placeholder=\"Detect from Simfile\">"
				"</div>"
				// Global time limit
				"<div class=\"field-group\">"
					"<label>Global time limit [s] (for each test)</label>"
					"<input type=\"text\" name=\"time-limit\" value=\"",
						htmlEscape(time_limit), "\" size=\"24\" "
					"placeholder=\"No global time limit\">"
				"</div>"
				// Force auto limit
				"<div class=\"field-group\">"
					"<label>Force automatic time limit setting</label>"
					"<input type=\"checkbox\" name=\"force-auto-limit\"",
						(force_auto_limit ? " checked" : ""), ">"
				"</div>"
				// Ignore Simfile
				"<div class=\"field-group\">"
					"<label>Ignore Simfile</label>"
					"<input type=\"checkbox\" name=\"ignore-simfile\"",
						(ignore_simfile ? " checked" : ""), ">"
				"</div>"
				// Package
				"<div class=\"field-group\">"
					"<label>Package</label>"
					"<input type=\"file\" name=\"package\" required>"
				"</div>"
				"<input class=\"btn blue\" type=\"submit\" value=\"Add\">"
			"</form>"
		"</div>");
}

void Contest::editContest() {
	if (!rpath->admin_access)
		return error403();

	FormValidator fv(req->form_data);
	string name, owner;
	bool is_public, show_ranking;

	if (req->method == server::HttpRequest::POST) {
		if (fv.get("csrf_token") != Session::csrf_token)
			return error403();

		// Validate all fields
		fv.validateNotBlank(name, "name", "Contest name", ROUND_NAME_MAX_LEN);

		fv.validateNotBlank(owner, "owner", "Owner username", isUsername,
			"Username can only consist of characters [a-zA-Z0-9_-]",
			USERNAME_MAX_LEN);

		is_public = fv.exist("public");
		show_ranking = fv.exist("show-ranking");

		try {
			DB::Statement stmt;
			// Check if user has the ability to make contest public
			if (is_public && Session::user_type > UTYPE_ADMIN
				&& !rpath->contest->is_public)
			{
				is_public = false;
				fv.addError("Only admins can make contest public");
			}

			// If all fields are ok
			if (fv.noErrors()) {
				stmt = db_conn.prepare("UPDATE rounds r, "
						"(SELECT id FROM users WHERE username=?) u "
					"SET name=?, owner=u.id, is_public=?, show_ranking=? "
					"WHERE r.id=?");
				stmt.setString(1, owner);
				stmt.setString(2, name);
				stmt.setBool(3, is_public);
				stmt.setBool(4, show_ranking);
				stmt.setString(5, rpath->round_id);

				if (stmt.executeUpdate() == 1) {
					fv.addError("Update successful");
					// Update rpath
					rpath.reset(getRoundPath(rpath->round_id));
					if (!rpath)
						return; // getRoundPath has already set an error

				} /*else // TODO: make it working
					fv.addError("User not found");*/
			}

		} catch (const std::exception& e) {
			ERRLOG_CATCH(e);
			fv.addError("Internal server error");
		}
	}

	// Get contest information
	DB::Statement stmt = db_conn.prepare(
		"SELECT u.username FROM rounds r, users u WHERE r.id=? AND owner=u.id");
	stmt.setString(1, rpath->round_id);

	DB::Result res = stmt.executeQuery();
	if (!res.next())
		THROW("Failed to get contest and owner info");

	name = rpath->contest->name;
	owner = res[1];
	is_public = rpath->contest->is_public;
	show_ranking = rpath->contest->show_ranking;

	contestTemplate("Edit contest");
	printRoundPath();
	append(fv.errors(), "<div class=\"form-container\">"
			"<h1>Edit contest</h1>"
			"<form method=\"post\">"
				// Name
				"<div class=\"field-group\">"
					"<label>Contest name</label>"
					"<input type=\"text\" name=\"name\" value=\"",
						htmlEscape(name), "\" size=\"24\" "
						"maxlength=\"", toStr(ROUND_NAME_MAX_LEN), "\" "
						"required>"
				"</div>"
				// Owner
				"<div class=\"field-group\">"
					"<label>Owner username</label>"
					"<input type=\"text\" name=\"owner\" value=\"",
						htmlEscape(owner), "\" size=\"24\" "
						"maxlength=\"", toStr(USERNAME_MAX_LEN), "\" "
						"required>"
				"</div>"
				// Public
				"<div class=\"field-group\">"
					"<label>Public</label>"
					"<input type=\"checkbox\" name=\"public\"",
						(is_public ? " checked"
							: (Session::user_type > UTYPE_ADMIN ? " disabled"
								: "")), ">"
				"</div>"
				// Show ranking
				"<div class=\"field-group\">"
					"<label>Show ranking</label>"
					"<input type=\"checkbox\" name=\"show-ranking\"",
						(show_ranking ? " checked" : ""), ">"
				"</div>"
				"<div class=\"button-row\">"
					"<input class=\"btn blue\" type=\"submit\" value=\"Update\">"
					"<a class=\"btn red\" href=\"/c/", rpath->round_id,
						"/delete\">Delete contest</a>"
				"</div>"
			"</form>"
		"</div>");
}

void Contest::editRound() {
	if (!rpath->admin_access)
		return error403();

	FormValidator fv(req->form_data);
	string name;
	bool is_visible = false;
	string begins, full_results, ends;

	if (req->method == server::HttpRequest::POST) {
		if (fv.get("csrf_token") != Session::csrf_token)
			return error403();

		// Validate all fields
		fv.validateNotBlank(name, "name", "Round name", ROUND_NAME_MAX_LEN);

		is_visible = fv.exist("visible");

		fv.validate(begins, "begins", "Begins", isDatetime,
			"Begins: invalid value");// TODO: add length limit???????

		fv.validate(ends, "ends", "Ends", isDatetime, "Ends: invalid value");

		fv.validate(full_results, "full_results", "Ends", isDatetime,
			"Full_results: invalid value");

		// If all fields are ok
		if (fv.noErrors())
			try {
				DB::Statement stmt = db_conn.prepare("UPDATE rounds "
					"SET name=?, visible=?, begins=?, ends=?, full_results=? "
					"WHERE id=?");
				stmt.setString(1, name);
				stmt.setBool(2, is_visible);

				// Begins
				if (begins.empty())
					stmt.setNull(3);
				else
					stmt.setString(3, begins);

				// ends
				if (ends.empty())
					stmt.setNull(4);
				else
					stmt.setString(4, ends);

				// Full_results
				if (full_results.empty())
					stmt.setNull(5);
				else
					stmt.setString(5, full_results);

				stmt.setString(6, rpath->round_id);

				if (stmt.executeUpdate() == 1) {
					fv.addError("Update successful");
					// Update rpath
					rpath.reset(getRoundPath(rpath->round_id));
					if (!rpath)
						return; // getRoundPath has already set an error
				}

			} catch (const std::exception& e) {
				ERRLOG_CATCH(e);
				fv.addError("Internal server error");
			}
	}

	// Get round information
	name = rpath->round->name;
	is_visible = rpath->round->visible;
	begins = rpath->round->begins;
	ends = rpath->round->ends;
	full_results = rpath->round->full_results;

	contestTemplate("Edit round");
	printRoundPath();
	append(fv.errors(), "<div class=\"form-container\">"
			"<h1>Edit round</h1>"
			"<form method=\"post\">"
				// Name
				"<div class=\"field-group\">"
					"<label>Round name</label>"
					"<input type=\"text\" name=\"name\" value=\"",
						htmlEscape(name), "\" size=\"24\" "
						"maxlength=\"", toStr(ROUND_NAME_MAX_LEN), "\" "
						"required>"
				"</div>"
				// Visible before beginning
				"<div class=\"field-group\">"
					"<label>Visible before beginning</label>"
					"<input type=\"checkbox\" name=\"visible\"",
						(is_visible ? " checked" : ""), ">"
				"</div>"
				// Begins
				"<div class=\"field-group\">"
					"<label>Begins</label>"
					"<input type=\"text\" name=\"begins\""
						"placeholder=\"yyyy-mm-dd HH:MM:SS\" value=\"",
						htmlEscape(begins), "\" size=\"19\" "
						"maxlength=\"19\">"
					"<span>UTC</span>"
				"</div>"
				// Ends
				"<div class=\"field-group\">"
					"<label>Ends</label>"
					"<input type=\"text\" name=\"ends\""
						"placeholder=\"yyyy-mm-dd HH:MM:SS\" value=\"",
						htmlEscape(ends), "\" size=\"19\" "
						"maxlength=\"19\">"
					"<span>UTC</span>"
				"</div>"
				// Full_results
				"<div class=\"field-group\">"
					"<label>Full_results</label>"
					"<input type=\"text\" name=\"full_results\""
						"placeholder=\"yyyy-mm-dd HH:MM:SS\" value=\"",
						htmlEscape(full_results), "\" size=\"19\" "
						"maxlength=\"19\">"
					"<span>UTC</span>"
				"</div>"
				"<div class=\"button-row\">"
					"<input class=\"btn blue\" type=\"submit\" value=\"Update\">"
					"<a class=\"btn red\" href=\"/c/", rpath->round_id,
						"/delete\">Delete round</a>"
				"</div>"
			"</form>"
		"</div>");
}

void Contest::editProblem() {
	if (!rpath->admin_access)
		return error403();

	StringView next_arg = url_args.extractNextArg();
	// Rejudge
	if (next_arg == "rejudge") {
		try {
			DB::Statement stmt = db_conn.prepare("UPDATE submissions "
				"SET status=" SSTATUS_WAITING_STR ", queued=? "
				"WHERE problem_id=?");
			stmt.setString(1, date("%Y-%m-%d %H:%M:%S"));
			stmt.setString(2, rpath->problem->problem_id);

			stmt.executeUpdate();
			notifyJudgeServer();

		} catch (const std::exception& e) {
			ERRLOG_CATCH(e);
		}

		return redirect(concat("/c/", rpath->round_id, "/edit"));
	}

	// Download
	while (next_arg == "download") {
		constexpr std::array<unsigned char, 22> empty_zip_file{{
			0x50, 0x4b, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		}};

		// TODO: Simplify extension
		const char* _zip = ".zip";
		const char* _tgz = ".tar.gz";
		const char* extension;
		// Get extension
		next_arg = url_args.extractNextArg();
		if (next_arg == "zip")
			extension = _zip;
		else if (next_arg == "tgz")
			extension = _tgz;
		else
			break;

		// Create temporary file
		char tmp_file[] = "/tmp/sim-problem.XXXXXX";
		umask(077); // Only owner can access this temporary file
		int fd = mkstemp(tmp_file);
		if (fd == -1)
			THROW("Error: mkstemp('", tmp_file, "')", error(errno));

		sclose(fd);
		FileRemover remover(tmp_file);
		vector<string> args;
		// zip
		if (extension == _zip) {
			back_insert(args, "zip", "-rq", tmp_file,
				rpath->problem->problem_id);

			if (putFileContents(tmp_file, (const char*)empty_zip_file.data(),
				empty_zip_file.size()) == -1)
			{
				THROW("Error: putFileContents('", tmp_file, "')", error(errno));
			}
		// tar.gz
		} else // extension == tgz
			back_insert(args, "tar", "czf", tmp_file,
				rpath->problem->problem_id);

		// Compress package
		Spawner::ExitStat es;
		try {
			es = Spawner::run(args[0], args,
				{-1, STDERR_FILENO, STDERR_FILENO, 200 * 1000000 /* 200 s */},
				"problems");

		} catch (const std::exception& e) {
			ERRLOG_CATCH(e);
			return error500();
		}

		// TODO: better error handling
		if (es.code) {
			errlog("Error: ", args[0], ' ', es.message);
			return error500();
		}

		resp.content_type = server::HttpResponse::FILE_TO_REMOVE;
		resp.headers["Content-Disposition"] = concat("attachment; filename=",
			rpath->problem->problem_id, extension);
		resp.content = tmp_file;

		remover.cancel();
		return;
	}

	FormValidator fv(req->form_data);
	string round_name, name, label, memory_limit;

	if (req->method == server::HttpRequest::POST) {
		if (fv.get("csrf_token") != Session::csrf_token)
			return error403();

		// Validate all fields
		fv.validate(round_name, "round-name", "Round's name",
			ROUND_NAME_MAX_LEN);

		fv.validate(name, "name", "Problem name", PROBLEM_NAME_MAX_LEN);

		fv.validate(label, "label", "Label", PROBLEM_LABEL_MAX_LEN);

		fv.validateNotBlank(memory_limit, "memory-limit", "Memory limit",
			isDigitNotGreaterThan<(std::numeric_limits<uint64_t>::max() >> 20)>,
			"Memory limit: invalid value");

		// If all fields are ok
		if (fv.noErrors())
			try {
				// Update problem config
				sim::Simfile sf {getFileContents(concat("problems/",
					rpath->problem->problem_id, "/Simfile"))
				};

				sf.loadAll();

				sf.name = name;
				sf.label = label;

				// Update memory limit
				uint64_t new_mem_limit = strtoull(memory_limit) << 20;
				if (sf.global_mem_limit != new_mem_limit) {
					for (auto&& tg : sf.tgroups)
						for (auto&& t : tg.tests)
							if (t.memory_limit == sf.global_mem_limit)
								t.memory_limit = new_mem_limit;

					sf.global_mem_limit = new_mem_limit;
				}

				if (BLOCK_SIGNALS(
					putFileContents(
						concat("problems/", rpath->problem->problem_id,
							"/Simfile"),
						sf.dump())) == -1)
				{
					THROW("Failed to update problem ",
						rpath->problem->problem_id, " Simfile");
				}

				// Update database
				DB::Statement stmt = db_conn.prepare(
					"UPDATE rounds r, problems p "
					"SET r.name=?, p.name=?, p.label=? "
					"WHERE r.id=? AND p.id=?");
				stmt.setString(1, round_name);
				stmt.setString(2, name);
				stmt.setString(3, label);
				stmt.setString(4, rpath->round_id);
				stmt.setString(5, rpath->problem->problem_id);

				if (stmt.executeUpdate()) {
					fv.addError("Update successful"); // TODO: does not show in every case

					// Update rpath
					rpath.reset(getRoundPath(rpath->round_id));
					if (!rpath)
						return; // getRoundPath has already set an error
				}

			} catch (const std::exception& e) {
				ERRLOG_CATCH(e);
				fv.addError("Internal server error");
			}
	}

	// Get problem information
	round_name = rpath->problem->name;
	string simfile_contents = getFileContents(StringBuff<PATH_MAX>("problems/",
		rpath->problem->problem_id, "/Simfile"));
	// Read Simfile
	try {
		sim::Simfile sf {simfile_contents};
		sf.loadName();
		sf.loadLabel();
		sf.loadTests();
		name = sf.name;
		label = sf.label;
		memory_limit.clear();
		memory_limit += toStr(sf.global_mem_limit >> 20);

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return error500();
	}

	contestTemplate("Edit problem");
	printRoundPath();
	append(fv.errors(), "<div class=\"right-flow\" style=\"width:85%\">"
			"<div class=\"dropmenu down\">"
				"<a class=\"btn-small dropmenu-toggle\">"
					"Download package as<span class=\"caret\"></span></a>"
				"<ul>"
					"<a href=\"/c/", rpath->round_id, "/edit/download/zip\">"
						".zip</a>"
					"<a href=\"/c/", rpath->round_id, "/edit/download/tgz\">"
						".tar.gz</a>"
				"</ul>"
			"</div>"
			"<a class=\"btn-small blue\" href=\"/c/", rpath->round_id,
				"/edit/rejudge\">Rejudge all submissions</a>"
		"</div>"
		"<div class=\"form-container\">"
			"<h1>Edit problem</h1>"
			"<form method=\"post\">"
				// Round's name
				"<div class=\"field-group\">"
					"<label>Round's name</label>"
					"<input type=\"text\" name=\"round-name\" value=\"",
						htmlEscape(round_name), "\" size=\"24\" "
						"maxlength=\"", toStr(ROUND_NAME_MAX_LEN), "\" "
						"required>"
				"</div>"
				// Problem's name
				"<div class=\"field-group\">"
					"<label>Problem's name</label>"
					"<input type=\"text\" name=\"name\" value=\"",
						htmlEscape(name), "\" size=\"24\" "
						"maxlength=\"", toStr(PROBLEM_NAME_MAX_LEN), "\" "
						"required>"
				"</div>"
				// Problem's label
				"<div class=\"field-group\">"
					"<label>Problem's label</label>"
					"<input type=\"text\" name=\"label\" value=\"",
						htmlEscape(label), "\" size=\"24\" "
						"maxlength=\"", toStr(PROBLEM_LABEL_MAX_LEN),
						"\" required>"
				"</div>"
				// Memory limit
				"<div class=\"field-group\">"
					"<label>Memory limit [MB]</label>"
					"<input type=\"text\" name=\"memory-limit\" value=\"",
						htmlEscape(memory_limit), "\" size=\"24\" "
						"required>"
				"</div>"
				// TODO: Main solution
				"<div class=\"button-row\">"
					"<input class=\"btn blue\" type=\"submit\" value=\"Update\">"
					"<a class=\"btn red\" href=\"/c/", rpath->round_id,
						"/delete\">Delete problem</a>"
				"</div>"
			"</form>"
		"</div>"
		"<h2>Package Simfile:</h2>"
		"<pre class=\"simfile\">", simfile_contents, "</pre>");
}

void Contest::deleteContest() {
	if (!rpath->admin_access)
		return error403();

	FormValidator fv(req->form_data);
	if (req->method == server::HttpRequest::POST && fv.exist("delete"))
		try {
			if (fv.get("csrf_token") != Session::csrf_token)
				return error403();

			SignalBlocker signal_guard;
			// Delete submissions
			DB::Statement stmt = db_conn.prepare("DELETE FROM submissions "
				"WHERE contest_round_id=?");
			stmt.setString(1, rpath->round_id);
			stmt.executeUpdate();

			// Delete from contests_users
			stmt = db_conn.prepare("DELETE FROM contests_users "
				"WHERE contest_id=?");
			stmt.setString(1, rpath->round_id);
			stmt.executeUpdate();

			// Delete files (from disk)
			stmt = db_conn.prepare("SELECT * FROM files WHERE round_id=?");
			stmt.setString(1, rpath->round_id);
			DB::Result res {stmt.executeQuery()};
			while (res.next())
				// Ignore errors - there is no goo way to deal with them
				(void)unlink(concat("files/", res[1]));

			// Delete files (from database)
			stmt = db_conn.prepare("DELETE FROM files WHERE round_id=?");
			stmt.setString(1, rpath->round_id);
			stmt.executeUpdate();

			// Delete rounds
			stmt = db_conn.prepare("DELETE FROM rounds "
				"WHERE id=? OR parent=? OR grandparent=?");
			stmt.setString(1, rpath->round_id);
			stmt.setString(2, rpath->round_id);
			stmt.setString(3, rpath->round_id);

			if (stmt.executeUpdate())
				return redirect("/c");

		} catch (const std::exception& e) {
			ERRLOG_CATCH(e);
			fv.addError("Internal server error");
		}

	string referer = req->headers.get("Referer");
	if (referer.empty())
		referer = concat("/c/", rpath->round_id, "/edit");

	contestTemplate("Delete contest");
	printRoundPath();
	append(fv.errors(), "<div class=\"form-container\">"
		"<h1>Delete contest</h1>"
		"<form method=\"post\">"
			"<label class=\"field\">Are you sure you want to delete contest "
				"<a href=\"/c/", rpath->round_id, "\">",
				htmlEscape(rpath->contest->name), "</a>, all subrounds and"
				" submissions?</label>"
			"<div class=\"submit-yes-no\">"
				"<button class=\"btn red\" type=\"submit\" name=\"delete\">"
					"Yes, I'm sure</button>"
				"<a class=\"btn\" href=\"", referer, "\">No, go back</a>"
			"</div>"
		"</form>"
	"</div>");
}

void Contest::deleteRound() {
	if (!rpath->admin_access)
		return error403();

	FormValidator fv(req->form_data);
	if (req->method == server::HttpRequest::POST && fv.exist("delete"))
		try {
			if (fv.get("csrf_token") != Session::csrf_token)
				return error403();

			SignalBlocker signal_guard;
			// Delete submissions
			DB::Statement stmt = db_conn.prepare("DELETE FROM submissions "
					"WHERE parent_round_id=?");
			stmt.setString(1, rpath->round_id);
			stmt.executeUpdate();

			// Delete rounds
			stmt = db_conn.prepare("DELETE FROM rounds WHERE id=? OR parent=?");
			stmt.setString(1, rpath->round_id);
			stmt.setString(2, rpath->round_id);

			if (stmt.executeUpdate())
				return redirect("/c/" + rpath->contest->id);

		} catch (const std::exception& e) {
			ERRLOG_CATCH(e);
			fv.addError("Internal server error");
		}

	string referer = req->headers.get("Referer");
	if (referer.empty())
		referer = concat("/c/", rpath->round_id, "/edit");

	contestTemplate("Delete round");
	printRoundPath();
	append(fv.errors(), "<div class=\"form-container\">"
		"<h1>Delete round</h1>"
		"<form method=\"post\">"
			"<label class=\"field\">Are you sure you want to delete round "
				"<a href=\"/c/", rpath->round_id, "\">",
				htmlEscape(rpath->round->name), "</a>, all subrounds and"
				" submissions?</label>"
			"<div class=\"submit-yes-no\">"
				"<button class=\"btn red\" type=\"submit\" name=\"delete\">"
					"Yes, I'm sure</button>"
				"<a class=\"btn\" href=\"", referer, "\">No, go back</a>"
			"</div>"
		"</form>"
	"</div>");
}

void Contest::deleteProblem() {
	if (!rpath->admin_access)
		return error403();

	FormValidator fv(req->form_data);
	if (req->method == server::HttpRequest::POST && fv.exist("delete"))
		try {
			if (fv.get("csrf_token") != Session::csrf_token)
				return error403();

			SignalBlocker signal_guard;
			// Delete submissions
			DB::Statement stmt = db_conn.prepare(
				"DELETE FROM submissions WHERE round_id=?");
			stmt.setString(1, rpath->round_id);
			stmt.executeUpdate();

			// Delete problem round
			stmt = db_conn.prepare("DELETE FROM rounds WHERE id=?");
			stmt.setString(1, rpath->round_id);

			if (stmt.executeUpdate())
				return redirect("/c/" + rpath->round->id);

		} catch (const std::exception& e) {
			ERRLOG_CATCH(e);
			fv.addError("Internal server error");
		}

	string referer = req->headers.get("Referer");
	if (referer.empty())
		referer = concat("/c/", rpath->round_id, "/edit");

	contestTemplate("Delete problem");
	printRoundPath();
	append(fv.errors(), "<div class=\"form-container\">"
		"<h1>Delete problem</h1>"
		"<form method=\"post\">"
			"<label class=\"field\">Are you sure you want to delete problem "
				"<a href=\"/c/", rpath->round_id, "\">",
				htmlEscape(rpath->problem->name), "</a> and all its"
				" submissions?</label>"
			"<div class=\"submit-yes-no\">"
				"<button class=\"btn red\" type=\"submit\" name=\"delete\">"
					"Yes, I'm sure</button>"
				"<a class=\"btn\" href=\"", referer, "\">No, go back</a>"
			"</div>"
		"</form>"
	"</div>");
}

void Contest::users() {
	if (!rpath->admin_access)
		return error403();
	if (rpath->type != RoundType::CONTEST)
		return error404();

	if (req->method == server::HttpRequest::POST) {
		StringView next_arg {url_args.extractNextArg()};

		enum { ADD, CHANGE_MODE, EXPEL } query;
		if (next_arg == "add")
			query = ADD;
		else if (next_arg == "change-mode")
			query = CHANGE_MODE;
		else if (next_arg == "expel")
			query = EXPEL;
		else
			return error404();

		FormValidator fv(req->form_data);
		if (fv.get("csrf_token") != Session::csrf_token)
			return response("403 Forbidden");

		try {
			// Add user to the contest
			if (query == ADD) {
				string user, tmp;
				enum { USERNAME, UID } user_value_type;

				// user_value_type
				fv.validateNotBlank(tmp, "user_value_type", "user_value_type",
					meta::string("username").size());

				if (tmp == "uid")
					user_value_type = UID;
				else if (tmp == "username")
					user_value_type = USERNAME;
				else
					return response("501 Not Implemented");

				// User
				if (user_value_type == UID) {
					fv.validateNotBlank<bool(const StringView&)>(user, "user",
						"User id", isDigit,
						"User id has to be a positive integer");
				} else { // Username
					fv.validateNotBlank(user, "user", "Username", isUsername,
						"Username can only consist of characters [a-zA-Z0-9_-]",
						USERNAME_MAX_LEN);
				}

				// Mode
				uint mode;
				fv.validateNotBlank(tmp, "mode", "mode", 1);
				if (tmp == "c")
					mode = CU_MODE_CONTESTANT;
				else if (tmp == "m")
					mode = CU_MODE_MODERATOR;
				else
					return response("501 Not Implemented");

				// If any error was encountered
				if (!fv.noErrors())
					return response("400 Bad Request", fv.errors());

				// Add new user
				DB::Statement stmt = db_conn.prepare(concat("INSERT IGNORE "
						"contests_users (user_id, contest_id, mode) "
					"SELECT id, ?, ? FROM users WHERE ",
						(user_value_type == UID ? "id=?" : "username=?")));
				stmt.setString(1, rpath->round_id);
				stmt.setUInt(2, mode);
				stmt.setString(3, user);

				if (!stmt.executeUpdate())
					return response("400 Bad Request", "No such user found or "
						"the user has already been added");

				return response("200 OK");
			}

			// Change user mode
			if (query == CHANGE_MODE) {
				string uid, tmp;
				// Uid
				fv.validateNotBlank<bool(const StringView&)>(uid, "uid",
					"User id", isDigit, "User id has to be a positive integer");

				// Mode
				uint mode;
				fv.validateNotBlank(tmp, "mode", "mode", 1);
				if (tmp == "c")
					mode = CU_MODE_CONTESTANT;
				else if (tmp == "m")
					mode = CU_MODE_MODERATOR;
				else
					return response("501 Not Implemented");

				// If any error was encountered
				if (!fv.noErrors())
					return response("400 Bad Request", fv.errors());

				DB::Statement stmt = db_conn.prepare("UPDATE contests_users "
					"SET mode=? WHERE user_id=? AND contest_id=?");
				stmt.setUInt(1, mode);
				stmt.setString(2, uid);
				stmt.setString(3, rpath->round_id);
				stmt.executeUpdate();

				return response("200 OK"); // Ignore errors
			}

			// Expel user from the contest
			if (query == EXPEL) {
				string uid;
				// Uid
				fv.validateNotBlank<bool(const StringView&)>(uid, "uid",
					"User id", isDigit, "User id has to be a positive integer");

				// If any error was encountered
				if (!fv.noErrors())
					return response("400 Bad Request", fv.errors());

				DB::Statement stmt = db_conn.prepare(
					"DELETE FROM contests_users "
					"WHERE user_id=? AND contest_id=?");
				stmt.setString(1, uid);
				stmt.setString(2, rpath->round_id);

				if (!stmt.executeUpdate())
					return response("400 Bad Request", "No such user found");

				return response("200 OK");
			}

		} catch (const std::exception& e) {
			ERRLOG_CATCH(e);
			return response("500 Internal Server Error");
		}
	}

	contestTemplate("Contest users");
	append("<h1>Users assigned to the contest: ",
			htmlEscape(rpath->contest->name), "</h1>"
		"<button class=\"btn\" style=\"display:block\" "
			"onclick=\"addContestUser(", rpath->round_id, ")\">Add user"
		"</button>");
	try {
		DB::Statement stmt = db_conn.prepare(
			"SELECT id, username, first_name, last_name, mode "
			"FROM users, contests_users "
			"WHERE contest_id=? AND id=user_id "
			"ORDER BY mode DESC, id ASC");
		stmt.setString(1, rpath->round_id);
		DB::Result res = stmt.executeQuery();

		append("<table class=\"contest-users\">"
			"<thead>"
				"<tr>"
					"<th class=\"uid\">Uid</th>"
					"<th class=\"username\">Username</th>"
					"<th class=\"first-name\">First name</th>"
					"<th class=\"last-name\">Last name</th>"
					"<th class=\"mode\">Mode</th>"
					"<th class=\"actions\">Actions</th>"
				"</tr>"
			"</thead>"
			"<tbody>");

		while (res.next()) {
			string uid = res[1];
			string uname = res[2];
			uint mode = res.getUInt(5);

			append("<tr>"
				"<td>", uid, "</td>"
				"<td>", htmlEscape(uname), "</td>"
				"<td>", htmlEscape(res[3]), "</td>"
				"<td>", htmlEscape(res[4]), "</td>");

			switch (mode) {
			case CU_MODE_MODERATOR:
				append("<td class=\"moderator\">Moderator</td>");
				break;
			default:
				append("<td class=\"contestant\">Contestant</td>");
			}

			append("<td>"
					"<a class=\"btn-small\" href=\"/u/", uid, "\">"
						"View profile</a>"
					"<a class=\"btn-small orange\" "
						"onclick=\"changeContestUserMode(", rpath->round_id,
							",", uid, ",'",
							(mode == CU_MODE_MODERATOR ? "m" : "c"), "')\">"
						"Change mode</a>"
					"<a class=\"btn-small red\" onclick=\"expelContestUser(",
							rpath->round_id, ",", uid, ",'", uname, "')\">"
						"Expel</a>");

			append("</td>"
				"</tr>");
		}

		append("</tbody>"
			"</table>");

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return error500();
	}
}

void Contest::listProblems(bool admin_view) {
	contestTemplate("Problems");
	append("<h1>Problems</h1>");
	printRoundPath("problems", !admin_view);
	printRoundView(true, admin_view);
}

template<class T>
static typename T::const_iterator findWithId(const T& x, const string& id)
	noexcept
{
	auto beg = x.begin(), end = x.end();
	while (beg != end) {
		auto mid = beg + ((end - beg) >> 1);
		if (mid->get().id < id)
			beg = ++mid;
		else
			end = mid;
	}
	return (beg != x.end() && beg->get().id == id ? beg : x.end());
}

void Contest::ranking(bool admin_view) {
	if (!admin_view && !rpath->contest->show_ranking)
		return error403();

	contestTemplate("Ranking");
	append("<h1>Ranking</h1>");
	printRoundPath("ranking", !admin_view);

	struct RankingProblem {
		uint64_t id;
		string label;

		explicit RankingProblem(uint64_t i = 0, const string& abbr = "")
			: id(i), label(abbr) {}
	};

	struct RankingRound {
		string id, name, item;
		vector<RankingProblem> problems;

		explicit RankingRound(const string& a = "", const string& b = "",
			const string& c = "", const vector<RankingProblem>& d = {})
			: id(a), name(b), item(c), problems(d) {}

		bool operator<(const RankingRound& x) const {
			return StrNumCompare()(item, x.item);
		}
	};

	struct RankingField {
		uint64_t round_id;
		string submission_id, score;

		explicit RankingField(uint64_t ri, const string& si = "",
				const string& s = "")
			: round_id(ri), submission_id(si), score(s) {}
	};

	struct RankingRow {
		string user_id, name;
		int64_t score;
		vector<RankingField> fields;

		explicit RankingRow(const string& ui = "", const string& n = "",
			int64_t s = 0, const vector<RankingField>& f = {})
			: user_id(ui), name(n), score(s), fields(f) {}
	};

	try {
		DB::Statement stmt;
		DB::Result res;
		string current_time = date("%Y-%m-%d %H:%M:%S");

		// Select rounds
		const char* column = (rpath->type == CONTEST ? "parent" : "id");
		stmt = db_conn.prepare(admin_view ?
			concat("SELECT id, name, item FROM rounds WHERE ", column, "=?")
			: concat("SELECT id, name, item FROM rounds WHERE ", column, "=? "
				"AND (begins IS NULL OR begins<=?) "
				"AND (full_results IS NULL OR full_results<=?)"));

		stmt.setString(1, rpath->type == CONTEST
			? rpath->round_id
			: rpath->round->id);
		if (!admin_view) {
			stmt.setString(2, current_time);
			stmt.setString(3, current_time);
		}
		res = stmt.executeQuery();

		vector<RankingRound> rounds;
		rounds.reserve(res.rowCount()); // Needed for pointers validity
		vector<std::reference_wrapper<RankingRound>> rounds_by_id;
		while (res.next()) {
			rounds.emplace_back(
				res[1],
				res[2],
				res[3]
			);
			rounds_by_id.emplace_back(rounds.back());
		}
		if (rounds.empty()) {
			append("<p>There is no one in the ranking yet...</p>");
			return;
		}

		sort(rounds_by_id.begin(), rounds_by_id.end(),
			[](const RankingRound& a, const RankingRound& b) {
				return a.id < b.id;
			});

		// Select problems
		column = (rpath->type == CONTEST ? "grandparent" :
			(rpath->type == ROUND ? "parent" : "id"));
		stmt = db_conn.prepare(admin_view ?
			concat("SELECT r.id, label, parent FROM rounds r, problems p "
				"WHERE r.", column, "=? AND problem_id=p.id ORDER BY item")
			: concat("SELECT r.id, label, r.parent "
				"FROM rounds r, rounds r1, problems p "
				"WHERE r.", column, "=? AND r.problem_id=p.id "
					"AND r.parent=r1.id "
					"AND (r1.begins IS NULL OR r1.begins<=?)"
					"AND (r1.full_results IS NULL OR r1.full_results<=?)"));
		stmt.setString(1, rpath->round_id);
		if (!admin_view) {
			stmt.setString(2, current_time);
			stmt.setString(3, current_time);
		}
		res = stmt.executeQuery();

		// Add problems to rounds
		while (res.next()) {
			auto it = findWithId(rounds_by_id, res[3]);
			if (it == rounds_by_id.end())
				continue; // Ignore invalid rounds hierarchy

			it->get().problems.emplace_back(
				res.getUInt64(1),
				res[2]
			);
		}

		rounds_by_id.clear(); // Free unused memory
		sort(rounds.begin(), rounds.end());

		// Select submissions
		column = (rpath->type == CONTEST ? "contest_round_id" :
			(rpath->type == ROUND ? "parent_round_id" : "round_id"));
		stmt = db_conn.prepare(admin_view ?
			concat("SELECT s.id, user_id, u.first_name, u.last_name, round_id, "
					"score "
				"FROM submissions s, users u "
				"WHERE s.", column, "=? AND s.type=" STYPE_FINAL_STR " "
					"AND user_id=u.id "
				"ORDER BY user_id")
			: concat("SELECT s.id, user_id, u.first_name, u.last_name, "
					"round_id, score "
				"FROM submissions s, users u, rounds r "
				"WHERE s.", column, "=? AND s.type=" STYPE_FINAL_STR " "
					"AND user_id=u.id "
					"AND r.id=parent_round_id "
					"AND (begins IS NULL OR begins<=?) "
					"AND (full_results IS NULL OR full_results<=?) "
				"ORDER BY user_id"));
		stmt.setString(1, rpath->round_id);
		if (!admin_view) {
			stmt.setString(2, current_time);
			stmt.setString(3, current_time);
		}
		res = stmt.executeQuery();

		// Construct rows
		vector<RankingRow> rows;
		string last_user_id;
		while (res.next()) {
			// Next user
			if (last_user_id != res[2]) {
				rows.emplace_back(
					res[2],
					concat(res[3], ' ', res[4]),
					0
				);
			}
			last_user_id = rows.back().user_id;

			rows.back().score += res.getInt64(6);
			rows.back().fields.emplace_back(
				res.getUInt64(5),
				res[1],
				res[6]
			);
		}

		// Sort rows
		vector<std::reference_wrapper<RankingRow> > sorted_rows;
		sorted_rows.reserve(rows.size());
		for (size_t i = 0; i < rows.size(); ++i)
			sorted_rows.emplace_back(rows[i]);
		sort(sorted_rows.begin(), sorted_rows.end(),
			[](const RankingRow& a, const RankingRow& b) {
				return a.score > b.score;
			});

		// Print rows
		if (rows.empty()) {
			append("<p>There is no one in the ranking yet...</p>");
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
		append("<table class=\"table ranking stripped\">"
			"<thead>"
				"<tr>"
					"<th rowspan=\"2\">#</th>"
					"<th rowspan=\"2\" style=\"min-width:120px\">User</th>"
					"<th rowspan=\"2\">Sum</th>");
		// Rounds
		for (auto& i : rounds) {
			if (i.problems.empty())
				continue;

			append("<th");
			if (i.problems.size() > 1)
				append(" colspan=\"", toStr(i.problems.size()), '"');
			append("><a href=\"/c/", i.id,
				(admin_view ? "/ranking\">" : "/n/ranking\">"),
				htmlEscape(i.name), "</a></th>");
		}
		// Problems
		append(
			"</tr>"
			"<tr>");
		for (auto& i : rounds)
			for (auto& j : i.problems)
				append("<th><a href=\"/c/", toStr(j.id),
					(admin_view ? "/ranking\">" : "/n/ranking\">"),
					htmlEscape(j.label), "</a></th>");
		append("</tr>"
			"</thead>"
			"<tbody>");
		// Rows
		throw_assert(sorted_rows.size());
		size_t place = 1; // User place
		int64_t last_user_score = sorted_rows.front().get().score;
		vector<RankingField*> row_points(idx); // idx is now number of problems
		for (size_t i = 0, end = sorted_rows.size(); i < end; ++i) {
			RankingRow& row = sorted_rows[i];
			// Place
			if (last_user_score != row.score)
				place = i + 1;
			last_user_score = row.score;
			append("<tr>"
					"<td>", toStr(place), "</td>");
			// Name
			if (admin_view)
				append("<td><a href=\"/u/", row.user_id, "\">",
					htmlEscape(row.name), "</a></td>");
			else
				append("<td>", htmlEscape(row.name), "</td>");

			// Score
			append("<td>", toStr(row.score), "</td>");

			// Fields
			fill(row_points.begin(), row_points.end(), nullptr);
			for (auto& j : row.fields) {
				auto it = binaryFindBy(index_of, &pair<size_t, size_t>::first,
					j.round_id);
				if (it == index_of.end())
					THROW("Failed to get index of a problem");

				row_points[it->second] = &j;
			}
			for (auto& j : row_points) {
				if (j == nullptr)
					append("<td></td>");
				else if (admin_view || (Session::isOpen() &&
					row.user_id == Session::user_id))
				{
					append("<td><a href=\"/s/", j->submission_id, "\">",
						j->score, "</a></td>");
				} else
					append("<td>", j->score, "</td>");
			}

			append("</tr>");
		}
		append("</tbody>"
				"</thead>"
			"</table>");

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return error500();
	}
}

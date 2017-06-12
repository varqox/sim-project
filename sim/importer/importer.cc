#include <iostream>
#include <map>
#include <sim/constants.h>
#include <sim/mysql.h>
#include <sim/sqlite.h>
#include <simlib/process.h>
#include <simlib/random.h>
#include <simlib/sha.h>
#include <simlib/spawner.h>
#include <simlib/utilities.h>

using std::array;
using std::cin;
using std::cout;
using std::endl;
using std::map;
using std::pair;
using std::string;
using std::unique_ptr;
using std::vector;

SQLite::Connection importer_db;
MySQL::Connection old_conn, new_conn;

string old_build;
string new_build;

string package_hash(string package_zip) {
	(void)remove_r("/tmp/xxd");

	vector<string> args = {
		"bash",
		"-c",
		concat("mkdir -p /tmp/xxd;"
			" unzip -q ", package_zip, " -d /tmp/xxd;"
			" (cd /tmp/xxd/*; find . ! -name Simfile -type f -exec md5sum {} \\; | sort -k 2) | md5sum")
	};

	FileDescriptor output {openUnlinkedTmpFile()};
	throw_assert(output >= 0);
	Spawner::ExitStat es = Spawner::run(args[0], args,
		{-1, output, output, {}, 1024 << 20});

	// Append zip's output to the the report
	(void)lseek(output, 0, SEEK_SET);
	return getFileContents(output);
}

template<class... Args>
pair<int, string> spawn(Args&&... args) {
	FileDescriptor output {openUnlinkedTmpFile()};
	throw_assert(output >= 0);

	vector<string> argv { args... };
	Spawner::ExitStat es = Spawner::run(argv[0], argv, {-1, output, 2});

	(void)lseek(output, 0, SEEK_SET);
	return {es.code, getFileContents(output)};
}

void scan_old_db() {
	importer_db.execute("BEGIN");
	importer_db.execute("DELETE FROM problem_mapping");
	importer_db.execute("DELETE FROM package_hash_mapping");

	importer_db.execute("INSERT INTO jobs VALUES('import_users', '')");

	// Scam problems
	MySQL::Result res {old_conn.executeQuery("SELECT id FROM problems WHERE EXISTS (SELECT id FROM rounds WHERE problem_id=problems.id)")};
	while (res.next())
		importer_db.execute("INSERT INTO jobs VALUES('import_problem', '" + res[1] + "')");

	importer_db.execute("INSERT INTO jobs VALUES('repair_problems', '')");
	importer_db.execute("INSERT INTO jobs VALUES('import_rounds', '')");
	importer_db.execute("INSERT INTO jobs VALUES('import_contests_users', '')");
	importer_db.execute("INSERT INTO jobs VALUES('import_submissions', '')");
	importer_db.execute("INSERT INTO jobs VALUES('import_files', '')");
	importer_db.execute("INSERT INTO jobs VALUES('import_sim_account', '')");
	importer_db.execute("COMMIT");
}

vector<pair<bool, string>> get_row(MySQL::Result& res) {
	vector<pair<bool, string>> row(res.impl()->getMetaData()->getColumnCount());
	for (uint i = 0; i < row.size(); ++i)
		row[i] = {res.isNull(i + 1), res.getString(i + 1)};
	return row;
}

void set_row(MySQL::Statement& stmt, const vector<pair<bool, string>>& row) {
	for (uint i = 0; i < row.size(); ++i) {
		if (row[i].first)
			stmt.setNull(i + 1);
		else
			stmt.setString(i + 1, row[i].second);
	}
}

// Constructs the insert query
string build_query(string table, uint columns) {
	string res = "REPLACE " + table + " VALUES(";
	for (uint i = 0; i < columns; ++i)
		res += "?,";
	res.back() = ')';
	return res;
}

void import_users() {
	auto res = old_conn.executeQuery("SELECT * FROM users WHERE id!=" SIM_ROOT_UID);
	while (res.next()) {
		auto stmt = new_conn.prepare(build_query("users", res.impl()->getMetaData()->getColumnCount()));
		set_row(stmt, get_row(res));
		stmt.executeUpdate();
	}
}

void import_problem(string id) {
	string hash = package_hash(concat(old_build, "/problems/", id, ".zip"));
	SQLite::Statement stmt {importer_db.prepare("SELECT newid FROM package_hash_mapping WHERE hash=?")};
	stmt.bindText(1, hash, SQLITE_STATIC);
	// If there already is a problem with the same hash
	if (stmt.step() == SQLITE_ROW) {
		string newid = stmt.getStr(0).to_string();
		stdlog("Mapping duplication: ", id, " -> ", newid);

		stmt.prepare(importer_db, "INSERT INTO problem_mapping VALUES(?,?)");
		stmt.bindText(1, id, SQLITE_STATIC);
		stmt.bindText(2, newid, SQLITE_STATIC);
		throw_assert(stmt.step() == SQLITE_DONE);

		return;
	}

	// There is no such a problem

	string exec_path = getExec(getpid());
	exec_path.erase(exec_path.begin() + exec_path.rfind('/'), exec_path.end());

	// Submit the problem
	auto p = spawn(exec_path + "/submit_problem.py",
		concat(old_build, "/problems/", id, ".zip"));
	if (p.first)
		THROW("Submitting the problem failed");

	// Get the new problem's id
	StringView x {p.second};
	x.removeLeading(isspace);
	x.removeTrailing(isspace);
	string new_id = x.to_string();

	importer_db.execute("BEGIN");
	stmt.prepare(importer_db, "INSERT INTO problem_mapping VALUES(?,?)");
	stmt.bindText(1, id, SQLITE_STATIC);
	stmt.bindText(2, new_id, SQLITE_STATIC);
	throw_assert(stmt.step() == SQLITE_DONE);

	stmt.prepare(importer_db, "INSERT INTO package_hash_mapping VALUES(?,?)");
	stmt.bindText(1, hash, SQLITE_STATIC);
	stmt.bindText(2, new_id, SQLITE_STATIC);
	throw_assert(stmt.step() == SQLITE_DONE);

	importer_db.execute("COMMIT");
}

void repair_problems() {
	SQLite::Statement stmt {importer_db.prepare("SELECT oldid, newid FROM problem_mapping")};
	while (stmt.step() == SQLITE_ROW) {
		string oldid = stmt.getStr(0).to_string();
		string newid = stmt.getStr(1).to_string();

		auto res = old_conn.executeQuery("SELECT owner FROM problems WHERE id=" + oldid);
		throw_assert(res.next());

		string owner = res.getString(1);
		new_conn.executeUpdate("UPDATE problems SET owner=" + owner + " WHERE id=" + newid);
	}
}

void import_rounds() {
	auto res = old_conn.executeQuery("SELECT * FROM rounds");
	while (res.next()) {
		auto stmt = new_conn.prepare(build_query("rounds", res.impl()->getMetaData()->getColumnCount()));
		auto row = get_row(res);

		if (!row[3].first) {
			// Remap problem_id
			auto stmt1 = importer_db.prepare("SELECT newid FROM problem_mapping WHERE oldid=" + row[3].second);
			throw_assert(stmt1.step() == SQLITE_ROW);
			row[3].second = stmt1.getStr(0).to_string();
		}

		set_row(stmt, row);
		stmt.executeUpdate();
	}
}

void import_constests_users() {
	auto res = old_conn.executeQuery("SELECT * FROM contests_users");
	while (res.next()) {
		auto stmt = new_conn.prepare(build_query("contests_users", res.impl()->getMetaData()->getColumnCount()));
		set_row(stmt, get_row(res));
		stmt.executeUpdate();
	}
}

void import_submissions() {
	auto res = old_conn.executeQuery("SELECT * FROM submissions WHERE type!=" STYPE_PROBLEM_SOLUTION_STR " ORDER BY id");
	while (res.next()) {
		auto stmt = new_conn.prepare(build_query("submissions", res.impl()->getMetaData()->getColumnCount()));
		auto row = get_row(res);

		string oldsid = row[0].second;
		row[0].second = "0"; // renumber ids

		// Remap problem_id
		string oldpid = row[2].second;
		auto stmt1 = importer_db.prepare("SELECT newid FROM problem_mapping WHERE oldid=" + row[2].second);
		throw_assert(stmt1.step() == SQLITE_ROW);
		row[2].second = stmt1.getStr(0).to_string();

		set_row(stmt, row);
		stmt.executeUpdate();
		string lid = new_conn.lastInsertId();

		stdlog("Importing submission ", oldsid, " -> ", lid);

		assert(copy(concat(old_build, "/solutions/", oldsid, ".cpp"),
			concat(new_build, "/solutions/", lid, ".cpp")) == 0);
	}
}

void import_files() {
	auto res = old_conn.executeQuery("SELECT * FROM files");
	while (res.next()) {
		auto stmt = new_conn.prepare(build_query("files", res.impl()->getMetaData()->getColumnCount()));
		auto row = get_row(res);
		string id = row[0].second;

		set_row(stmt, row);
		stmt.executeUpdate();

		assert(copy(concat(old_build, "/files/", id),
			concat(new_build, "/files/", id)) == 0);
	}
}

void import_sim_account() {
	auto res = old_conn.executeQuery("SELECT * FROM users WHERE id=" SIM_ROOT_UID);
	throw_assert(res.next());

	auto stmt = new_conn.prepare(build_query("users", res.impl()->getMetaData()->getColumnCount()));
	set_row(stmt, get_row(res));
	stmt.executeUpdate();
}

void handle_jobs() {
	SQLite::Statement stmt {importer_db.prepare(
		"SELECT rowid, type, aux_id FROM jobs ORDER BY rowid")};
	while (stmt.step() == SQLITE_ROW) {
		string rowid = stmt.getStr(0).to_string();
		string type = stmt.getStr(1).to_string();
		string aux_id = stmt.getStr(2).to_string();

		if (type == "import_users")
			import_users();
		else if (type == "import_problem")
			import_problem(aux_id);
		else if (type == "repair_problems")
			repair_problems();
		else if (type == "import_rounds")
			import_rounds();
		else if (type == "import_contests_users")
			import_constests_users();
		else if (type == "import_submissions")
			import_submissions();
		else if (type == "import_files")
			import_files();
		else if (type == "import_sim_account")
			import_sim_account();
		else
			THROW("Unexpected type: ", type);

		importer_db.execute("DELETE FROM jobs WHERE rowid=" + rowid);
	}
}

int main2(int argc, char **argv) {
	stdlog.use(stdout);

	assert(false); // TODO: Importing jobs is not implemented!!!, handle submission_id and problem_id appropriately!

	if (argc != 3) {
		errlog("You have to specify the path to the old sim installation as the first argument and the path to new installation as the second argument");
		return 1;
	}

	try {
		// Get connection
		old_conn = MySQL::createConnectionUsingPassFile(
			concat(old_build = argv[1], "/.db.config"));
		new_conn = MySQL::createConnectionUsingPassFile(
			concat(new_build = argv[2], "/.db.config"));
		importer_db = SQLite::Connection("importer.db",
			SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);

	} catch (const std::exception& e) {
		errlog("\033[31mFailed to connect to database\033[m - ", e.what());
		return 4;
	}

	importer_db.execute("PRAGMA journal_mode=WAL");
	importer_db.execute("CREATE TABLE IF NOT EXISTS jobs (type, aux_id)");
	importer_db.execute("CREATE TABLE IF NOT EXISTS problem_mapping ("
		"oldid INTEGER PRIMARY KEY,"
		"newid INTEGER)");
	importer_db.execute("CREATE TABLE IF NOT EXISTS package_hash_mapping ("
		"hash TEXT,"
		"newid INTEGER,"
		"UNIQUE(newid))");

	SQLite::Statement stmt = importer_db.prepare("SELECT COUNT(rowid) FROM jobs");
	assert(stmt.step() == SQLITE_ROW);
	int jobs = stmt.getInt(0);

	if (jobs == 0) {
		int c;
		do {
			cout << "No jobs to do. Do you want to scan the old database and import old sim? [y/n] ";
			c = cin.get();
		} while (!isIn(c, std::initializer_list<int>{'y', 'n', EOF}));

		if (c == 'n' || c == EOF)
			return 0;
		else
			scan_old_db();
	}

	handle_jobs();

	return 0;
}

int main(int argc, char **argv) {
	try {
		return main2(argc, argv);

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return 1;
	}
}

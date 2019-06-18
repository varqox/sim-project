#include <sim/constants.h>
#include <sim/mysql.h>
#include <simlib/spawner.h>

using std::map;
using std::pair;
using std::string;
using std::vector;

MySQL::Connection conn;
string build;

template <class... Args>
pair<int, string> spawn(Args&&... args) {
	FileDescriptor output {openUnlinkedTmpFile()};
	throw_assert(output >= 0);

	vector<string> argv {std::forward<Args>(args)...};
	Spawner::ExitStat es = Spawner::run(argv[0], argv, {-1, output, 2});

	(void)lseek(output, 0, SEEK_SET);
	return {es.code, getFileContents(output)};
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

#include <sim/jobs.h>

void work() {
	auto res = conn.executeQuery("SELECT * FROM submissions "
	                             "ORDER BY submit_time ASC, type ASC, id ASC");
	// Collect submissions
	stdlog("Collecting submissions...");
	map<string, int> M; // old_id => new_id
	vector<vector<pair<bool, string>>> data;
	vector<string> sources;
	while (res.next()) {
		data.emplace_back(get_row(res));
		string oldid = data.back()[0].second;
		sources.emplace_back(
		   getFileContents(concat(build, "/solutions/", oldid, ".cpp")));
		M[oldid] = data.size();
	}

	stdlog("Deleting submissions...");
	conn.executeUpdate("DELETE FROM submissions");
	// Delete sources
	forEachDirComponent(concat(build, "/solutions/"), [&](dirent* file) {
		remove(concat(build, "/solutions/", file->d_name));
	});

	// Insert submissions with new ids
	stdlog("Inserting submissions with new ids...");
	for (size_t i = 0; i < data.size(); ++i) {
		auto&& row = data[i];
		row[0].second = toStr(M[row[0].second]).str; // Set id to thew new one
		auto stmt = conn.prepare(build_query("submissions", row.size()));
		set_row(stmt, row);
		stmt.executeUpdate();

		putFileContents(concat(build, "/solutions/", row[0].second, ".cpp"),
		                sources[i]);
	}

	conn.executeUpdate(
	   concat("ALTER TABLE submissions AUTO_INCREMENT=", data.size()));

	// Update submissions' ids in jobs
	stdlog("Updating jobs...");
	res = conn.executeQuery(
	   "SELECT id, aux_id FROM jobs WHERE type=" JQTYPE_JUDGE_SUBMISSION_STR);
	auto stmt = conn.prepare("UPDATE jobs SET aux_id=? WHERE id=?");
	while (res.next()) {
		stmt.setString(1, toStr(M[res[2]]).str);
		stmt.setString(2, res[1]);
		stmt.executeUpdate();
	}
}

int main2(int argc, char** argv) {
	stdlog.use(stdout);

	if (argc != 2) {
		errlog("You have to specify the path to the sim installation as the "
		       "first argument");
		return 1;
	}

	try {
		// Get connection
		conn = MySQL::createConnectionUsingPassFile(
		   concat(build = argv[1], "/.db.config"));

	} catch (const std::exception& e) {
		errlog("\033[31mFailed to connect to database\033[m - ", e.what());
		return 4;
	}

	work();

	return 0;
}

int main(int argc, char** argv) {
	try {
		return main2(argc, argv);

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return 1;
	}
}

#include <iostream>
#include <map>
#include <sim/constants.h>
#include <sim/mysql.h>
#include <sim/sqlite.h>
#include <sim/submission.h>
#include <sim/utilities.h>
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

namespace old {

enum class SubmissionType : uint8_t {
	NORMAL = 0,
	FINAL = 1,
	VOID = 2,
	IGNORED = 3,
	PROBLEM_SOLUTION = 4,
};

// Initial and final values may be combined, but special not
enum class SubmissionStatus : uint8_t {
	// Final
	OK = 1,
	WA = 2,
	TLE = 3,
	MLE = 4,
	RTE = 5,
	FINAL_MASK = 7,
	// Initial
	INITIAL_OK  = OK << 3,
	INITIAL_WA  = WA << 3,
	INITIAL_TLE = TLE << 3,
	INITIAL_MLE = MLE << 3,
	INITIAL_RTE = RTE << 3,
	INITIAL_MASK = FINAL_MASK << 3,
	// Special
	PENDING                   = (8 << 3) + 0,
	// Fatal
	COMPILATION_ERROR         = (8 << 3) + 1,
	CHECKER_COMPILATION_ERROR = (8 << 3) + 2,
	JUDGE_ERROR               = (8 << 3) + 3
};
DECLARE_ENUM_UNARY_OPERATOR(SubmissionStatus, ~)
DECLARE_ENUM_OPERATOR(SubmissionStatus, |)
DECLARE_ENUM_OPERATOR(SubmissionStatus, &)

enum class JobQueueStatus : uint8_t {
	PENDING = 1,
	IN_PROGRESS = 2,
	DONE = 3,
	FAILED = 4,
	CANCELED = 5
};

} // namespace old

struct TransformRes {
	my_bool problem_final, contest_final, contest_initial_final;
	uint initial_status, full_status;
};

TransformRes submission_transform(uint& type, const uint status) {
	TransformRes res;
	res.problem_final = false;
	res.contest_final = false;
	res.contest_initial_final = false;

	switch (old::SubmissionType(type)) {
	case old::SubmissionType::NORMAL:
		type = uint(SubmissionType::NORMAL);
		break;
	case old::SubmissionType::VOID:
		type = uint(SubmissionType::VOID);
		break;
	case old::SubmissionType::FINAL:
		type = uint(SubmissionType::NORMAL);
		res.contest_final = true;
		break;
	case old::SubmissionType::IGNORED:
		type = uint(SubmissionType::IGNORED);
		break;
	case old::SubmissionType::PROBLEM_SOLUTION:
		type = uint(SubmissionType::PROBLEM_SOLUTION);
		break;
	}

	old::SubmissionStatus old_status = old::SubmissionStatus(status);
	if (old_status == old::SubmissionStatus::PENDING)
		res.initial_status = res.full_status = (uint)SubmissionStatus::PENDING;
	else if (old_status == old::SubmissionStatus::COMPILATION_ERROR)
		res.initial_status = res.full_status = (uint)SubmissionStatus::COMPILATION_ERROR;
	else if (old_status == old::SubmissionStatus::CHECKER_COMPILATION_ERROR)
		res.initial_status = res.full_status = (uint)SubmissionStatus::CHECKER_COMPILATION_ERROR;
	else if (old_status == old::SubmissionStatus::JUDGE_ERROR)
		res.initial_status = res.full_status = (uint)SubmissionStatus::JUDGE_ERROR;
	else {
		// Initial
		if ((old_status & old::SubmissionStatus::INITIAL_MASK) == old::SubmissionStatus::INITIAL_OK)
			res.initial_status = (uint)SubmissionStatus::OK;
		else if ((old_status & old::SubmissionStatus::INITIAL_MASK) == old::SubmissionStatus::INITIAL_WA)
			res.initial_status = (uint)SubmissionStatus::WA;
		else if ((old_status & old::SubmissionStatus::INITIAL_MASK) == old::SubmissionStatus::INITIAL_TLE)
			res.initial_status = (uint)SubmissionStatus::TLE;
		else if ((old_status & old::SubmissionStatus::INITIAL_MASK) == old::SubmissionStatus::INITIAL_MLE)
			res.initial_status = (uint)SubmissionStatus::MLE;
		else if ((old_status & old::SubmissionStatus::INITIAL_MASK) == old::SubmissionStatus::INITIAL_RTE)
			res.initial_status = (uint)SubmissionStatus::RTE;
		// Full
		if ((old_status & old::SubmissionStatus::FINAL_MASK) == old::SubmissionStatus::OK)
			res.full_status = (uint)SubmissionStatus::OK;
		else if ((old_status & old::SubmissionStatus::FINAL_MASK) == old::SubmissionStatus::WA)
			res.full_status = (uint)SubmissionStatus::WA;
		else if ((old_status & old::SubmissionStatus::FINAL_MASK) == old::SubmissionStatus::TLE)
			res.full_status = (uint)SubmissionStatus::TLE;
		else if ((old_status & old::SubmissionStatus::FINAL_MASK) == old::SubmissionStatus::MLE)
			res.full_status = (uint)SubmissionStatus::MLE;
		else if ((old_status & old::SubmissionStatus::FINAL_MASK) == old::SubmissionStatus::RTE)
			res.full_status = (uint)SubmissionStatus::RTE;
	}

	return res;
}

int main2(int argc, char **argv) {
	STACK_UNWINDING_MARK;

	stdlog.use(stdout);

	if (argc != 3) {
		errlog.label(false);
		errlog("You have to specify the path to the old sim installation as the first argument and the path to new installation as the second argument");
		return 1;
	}

	try {
		// Get connection
		auto old_conf = getFileByLines(concat_tostr(argv[1], "/.db.config"));
		putFileContents(concat_tostr(argv[1], "/.db.config.new"), intentionalUnsafeStringView(concat("user: ", old_conf[0],
			"password: ", old_conf[1], "db: ", old_conf[2], "host: ", old_conf[3])));
		old_conn = MySQL::make_conn_with_credential_file(
			concat_tostr(old_build = argv[1], "/.db.config.new"));
		new_conn = MySQL::make_conn_with_credential_file(
			concat_tostr(new_build = argv[2], "/.db.config"));
		// importer_db = SQLite::Connection("importer.db",
		// 	SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
	} catch (const std::exception& e) {
		errlog("\033[31mFailed to connect to database\033[m - ", e.what());
		return 4;
	}

	// Kill both webservers and job-servers to not interrupt with the upgrade
	auto killinstc = concat_tostr(getExecDir(getpid()), "/../src/killinstc");
	Spawner::run(killinstc, {
		killinstc,
		concat_tostr(old_build, "/sim-server"),
		concat_tostr(old_build, "/job-server"),
		concat_tostr(new_build, "/sim-server"),
		concat_tostr(new_build, "/job-server"),
	});

	constexpr size_t BUFF_LEN = 4 << 10;
	// Users
	{
		int64_t id;
		InplaceBuff<BUFF_LEN> username, first_name, last_name, email, salt, password, type;
		auto ostmt = old_conn.prepare("SELECT id, username, first_name, last_name, email, salt, password, type FROM users");
		ostmt.bindAndExecute();
		ostmt.res_bind_all(id, username, first_name, last_name, email, salt, password, type);

		new_conn.update("TRUNCATE TABLE users");
		auto nstmt = new_conn.prepare("INSERT INTO users(id, username, first_name, last_name, email, salt, password, type) VALUES(?,?,?,?,?,?,?,?)");
		while (ostmt.next())
			nstmt.bindAndExecute(id, username, first_name, last_name, email, salt, password, type);
	}

	// Session
	{
		InplaceBuff<BUFF_LEN> id, csrf_token, user_id, data, ip, user_agent, time;
		auto ostmt = old_conn.prepare("SELECT id, csrf_token, user_id, data, ip, user_agent, time FROM session");
		ostmt.bindAndExecute();
		ostmt.res_bind_all(id, csrf_token, user_id, data, ip, user_agent, time);

		new_conn.update("TRUNCATE TABLE session");
		auto nstmt = new_conn.prepare("INSERT INTO session(id, csrf_token, user_id, data, ip, user_agent, expires) VALUES(?,?,?,?,?,?,?)");
		while (ostmt.next())
			nstmt.bindAndExecute(id, csrf_token, user_id, data, ip, user_agent, time);
	}

	// Problems
	{
		forEachDirComponent(concat(new_build, "/problems"), [](dirent* file) {
			remove(intentionalUnsafeCStringView(concat(new_build, "/problems/", file->d_name)).data());
		});

		InplaceBuff<BUFF_LEN> id, type, name, label, simfile, owner, added, last_edit;
		auto ostmt = old_conn.prepare("SELECT id, type, name, label, simfile, owner, added, last_edit FROM problems");
		ostmt.bindAndExecute();
		ostmt.res_bind_all(id, type, name, label, simfile, owner, added, last_edit);

		new_conn.update("TRUNCATE TABLE problems");
		auto nstmt = new_conn.prepare("INSERT INTO problems(id, type, name, label, simfile, owner, added, last_edit) VALUES(?,?,?,?,?,?,?,?)");
		while (ostmt.next()) {
			nstmt.bindAndExecute(id, type, name, label, simfile, owner, added, last_edit);

			if (link(intentionalUnsafeCStringView(concat(old_build, "/problems/", id, ".zip")).data(),
				intentionalUnsafeCStringView(concat(new_build, "/problems/", id, ".zip")).data()))
				THROW("link()", errmsg());
		}
	}

	// Problem tags
	{
		int count;
		auto stmt = old_conn.prepare("SELECT count(*) FROM problems_tags");
		stmt.res_bind_all(count);
		stmt.bindAndExecute();
		throw_assert(stmt.next() and count == 0);
	}

	// Contests users
	{
		InplaceBuff<BUFF_LEN> user_id, contest_id, mode;
		auto ostmt = old_conn.prepare("SELECT user_id, contest_id, mode FROM contests_users");
		ostmt.bindAndExecute();
		ostmt.res_bind_all(user_id, contest_id, mode);

		new_conn.update("TRUNCATE TABLE contest_users");
		auto nstmt = new_conn.prepare("INSERT INTO contest_users(user_id, contest_id, mode) VALUES(?,?,?)");
		while (ostmt.next())
			nstmt.bindAndExecute(user_id, contest_id, mode);
	}

	// Rounds
	{
		MySQL::Optional<InplaceBuff<BUFF_LEN>> parent, grandparent, problem_id, begins, full_results, ends;
		InplaceBuff<BUFF_LEN> id, name, owner, item, is_public, visible;
		bool show_ranking;
		auto ostmt = old_conn.prepare("SELECT id, parent, grandparent, problem_id, name, owner, item, is_public, visible, show_ranking, begins, full_results, ends FROM rounds");
		ostmt.bindAndExecute();
		ostmt.res_bind_all(id, parent, grandparent, problem_id, name, owner, item, is_public, visible, show_ranking, begins, full_results, ends);

		new_conn.update("TRUNCATE TABLE contests");
		new_conn.update("TRUNCATE TABLE contest_rounds");
		new_conn.update("TRUNCATE TABLE contest_problems");
		auto ncstmt = new_conn.prepare("INSERT INTO contests(id, name, is_public) VALUES(?,?,?)");
		auto ncrstmt = new_conn.prepare("INSERT INTO contest_rounds(id, contest_id, name, item, begins, ends, full_results, ranking_exposure) VALUES(?,?,?,?,?,?,?,?)");
		auto ncpstmt = new_conn.prepare("INSERT INTO contest_problems(id, contest_round_id, contest_id, problem_id, name, item, final_selecting_method, reveal_score) VALUES(?,?,?,?,?,?,?,?)");
		auto custmt = new_conn.prepare("REPLACE INTO contest_users(user_id, contest_id, mode) VALUES(?, ?, " CU_MODE_OWNER_STR ")");

		map<decltype(id), bool> contest_id2show_ranking;
		while (ostmt.next()) {
			// Contest
			if (not parent.has_value()) {
				ncstmt.bindAndExecute(id, name, is_public);
				contest_id2show_ranking[id] = show_ranking;
				if (owner != "0")
					custmt.bindAndExecute(owner, id);
			// Contest round
			} else if (not grandparent.has_value()) {
				InfDatetime beg, end, full_res;
				if (begins.has_value())
					beg.set_datetime(begins.value().to_string());
				else
					beg.set_neg_inf();

				if (ends.has_value())
					end.set_datetime(ends.value().to_string());
				else
					end.set_inf();

				if (full_results.has_value())
					full_res.set_datetime(full_results.value().to_string());
				else
					full_res.set_neg_inf();

				InfDatetime ranking_exposure;
				throw_assert(contest_id2show_ranking.count(parent.value()) == 1);
				if (contest_id2show_ranking[parent.value()])
					ranking_exposure.set_neg_inf();
				else
					ranking_exposure.set_inf();

				ncrstmt.bindAndExecute(id, parent.value(), name, item, beg.to_str(), end.to_str(), full_res.to_str(), ranking_exposure.to_str());
			// Contest problem
			} else {
				ncpstmt.bindAndExecute(id, parent.value(), grandparent.value(), problem_id.value(), name, item, EnumVal<SubmissionFinalSelectingMethod>(SubmissionFinalSelectingMethod::LAST_COMPILING), false);
			}
		}
	}

	// Submissions
	{
		forEachDirComponent(concat(new_build, "/solutions"), [](dirent* file) {
			remove(intentionalUnsafeCStringView(concat(new_build, "/solutions/", file->d_name)).data());
		});

		MySQL::Optional<InplaceBuff<BUFF_LEN>> owner, round_id, parent_round_id, contest_round_id, score;
		InplaceBuff<BUFF_LEN> id, problem_id, submit_time, last_judgment, initial_report, final_report;
		uint type, status;

		auto ostmt = old_conn.prepare("SELECT id, owner, problem_id, round_id, parent_round_id, contest_round_id, type, status, submit_time, score, last_judgment, initial_report, final_report FROM submissions");
		ostmt.bindAndExecute();
		ostmt.res_bind_all(id, owner, problem_id, round_id, parent_round_id, contest_round_id, type, status, submit_time, score, last_judgment, initial_report, final_report);

		new_conn.update("TRUNCATE TABLE submissions");
		auto nstmt = new_conn.prepare("INSERT INTO submissions(id, owner, problem_id, contest_problem_id, contest_round_id, contest_id, type, language, final_candidate, problem_final, contest_final, contest_initial_final, initial_status, full_status, submit_time, score, last_judgment, initial_report, final_report) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
		while (ostmt.next()) {
			auto res = submission_transform(type, status);
			bool final_candidate = (not is_fatal(EnumVal<SubmissionStatus>(res.full_status)) and EnumVal<SubmissionType>(type) == SubmissionType::NORMAL);
			nstmt.bindAndExecute(id, owner, problem_id, round_id, parent_round_id, contest_round_id, type, EnumVal<SubmissionLanguage>(SubmissionLanguage::CPP), final_candidate, res.problem_final, res.contest_final, res.contest_initial_final, res.initial_status, res.full_status, submit_time, score, last_judgment, initial_report, final_report);

			StringView owner_str, round_id_str;
			if (owner.has_value())
				owner_str = owner.value();
			if (round_id.has_value())
				round_id_str = round_id.value();

			submission::update_final(new_conn, owner_str, problem_id, round_id_str);

			if (link(intentionalUnsafeCStringView(concat(old_build, "/solutions/", id, ".cpp")).data(),
				intentionalUnsafeCStringView(concat(new_build, "/solutions/", id)).data()))
				THROW("link()", errmsg());
		}
	}

	// Files
	{
		forEachDirComponent(concat(new_build, "/files"), [](dirent* file) {
			remove(intentionalUnsafeCStringView(concat(new_build, "/files/", file->d_name)).data());
		});

		InplaceBuff<BUFF_LEN> id, round_id, name, description, file_size, modified;
		auto ostmt = old_conn.prepare("SELECT id, round_id, name, description, file_size, modified FROM files");
		ostmt.bindAndExecute();
		ostmt.res_bind_all(id, round_id, name, description, file_size, modified);

		new_conn.update("TRUNCATE TABLE files");
		auto nstmt = new_conn.prepare("INSERT INTO files(id, contest_id, name, description, file_size, modified, creator) VALUES(?,?,?,?,?,?,?)");
		while (ostmt.next()) {
			nstmt.bindAndExecute(id, round_id, name, description, file_size, modified, nullptr);

			if (link(intentionalUnsafeCStringView(concat(old_build, "/files/", id)).data(),
				intentionalUnsafeCStringView(concat(new_build, "/files/", id)).data()))
				THROW("link()", errmsg());
		}
	}

	// Job queue
	{
		forEachDirComponent(concat(new_build, "/jobs_files"), [](dirent* file) {
			remove(intentionalUnsafeCStringView(concat(new_build, "/jobs_files/", file->d_name)).data());
		});

		MySQL::Optional<InplaceBuff<BUFF_LEN>> creator, aux_id;
		InplaceBuff<BUFF_LEN> id, type, priority, added, info, data;
		uint status;

		auto ostmt = old_conn.prepare("SELECT id, creator, type, priority, status, added, aux_id, info, data FROM job_queue");
		ostmt.bindAndExecute();
		ostmt.res_bind_all(id, creator, type, priority, status, added, aux_id, info, data);

		new_conn.update("TRUNCATE TABLE jobs");
		auto nstmt = new_conn.prepare("INSERT INTO jobs(id, creator, type, priority, status, added, aux_id, info, data) VALUES(?,?,?,?,?,?,?,?,?)");
		while (ostmt.next()) {
			switch (old::JobQueueStatus(status)) {
			case old::JobQueueStatus::PENDING:
				status = uint(JobStatus::PENDING);
				break;
			case old::JobQueueStatus::IN_PROGRESS:
				status = uint(JobStatus::IN_PROGRESS);
				break;
			case old::JobQueueStatus::DONE:
				status = uint(JobStatus::DONE);
				break;
			case old::JobQueueStatus::FAILED:
				status = uint(JobStatus::FAILED);
				break;
			case old::JobQueueStatus::CANCELED:
				status = uint(JobStatus::CANCELED);
				break;
			}

			nstmt.bindAndExecute(id, creator, type, priority, status, added, aux_id, info, data);

			if (link(intentionalUnsafeCStringView(concat(old_build, "/jobs_files/", id, ".zip")).data(),
				intentionalUnsafeCStringView(concat(new_build, "/jobs_files/", id, ".zip")).data()) and errno != ENOENT)
				THROW("link()", errmsg());
		}
	}

	// assert(false);

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

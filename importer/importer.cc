#include <deque>
#include <iostream>
#include <map>
#include <sim/constants.h>
#include <sim/jobs.h>
#include <sim/mysql.h>
#include <sim/sqlite.h>
#include <sim/submission.h>
#include <sim/utilities.h>
#include <simlib/libzip.h>
#include <simlib/process.h>
#include <simlib/random.h>
#include <simlib/sha.h>
#include <simlib/sim/problem_package.h>
#include <simlib/sim/simfile.h>
#include <simlib/spawner.h>
#include <simlib/utilities.h>
#include <unistd.h>

using std::array;
using std::cin;
using std::cout;
using std::deque;
using std::endl;
using std::map;
using std::pair;
using std::string;
using std::unique_ptr;
using std::vector;

template<class IterA, class IterB, class FuncA, class FuncB, class Compare>
void merge(IterA a_beg, IterA a_end, IterB b_beg, IterB b_end, FuncA&& a_merge, FuncB&& b_merge, Compare&& is_left_lower) {
	while (a_beg != a_end or b_beg != b_end) {
		if (a_beg == a_end) {
			b_merge(*b_beg);
			++b_beg;
		} else if (b_beg == b_end) {
			a_merge(*a_beg);
			++a_beg;
		} else if (is_left_lower(*a_beg, *b_beg)) {
			a_merge(*a_beg);
			++a_beg;
		} else {
			b_merge(*b_beg);
			++b_beg;
		}
	}
}

SQLite::Connection importer_db;
MySQL::Connection a_conn, b_conn, new_conn;

string a_build;
string b_build;
string new_build;

struct User {
	int64_t id;
	InplaceBuff<40> username, first_name, last_name, email, salt, password;
	int64_t type;

	User* replacer = nullptr;
};

map<int64_t, int64_t> skipped_a_users, skipped_b_users;
map<int64_t, User> a_users, b_users, new_users; // (user_id -> user)

// Works in O(n^2) where n is the total number of users
void load_users() {
	auto read_users = [&, nid = 0ll](auto& conn, auto& skipped_users, auto& users_map) mutable {
		User u;
		auto stmt = conn.prepare("SELECT id, username, first_name, last_name, email, salt, password, type FROM users ORDER BY id");
		stmt.bindAndExecute();
		stmt.res_bind_all(u.id, u.username, u.first_name, u.last_name, u.email, u.salt, u.password, u.type);

		int64_t last_uid = 0;
		while (stmt.next()) {
			while (++last_uid < u.id)
				skipped_users.emplace(last_uid, ++nid);

			u.replacer = nullptr;

			// Check if there is already another user which might be a candidate for merging
			for (auto&& p : new_users) {
				auto& nu = p.second;
				if ((u.first_name == nu.first_name and u.last_name == nu.last_name) or
					u.username == nu.username)
				{
					if (u.replacer != nullptr) {
						THROW("User collision was found: ",
							u.username, ", ", u.first_name, ", ", u.last_name,
							" matches with ",
								u.replacer->username, ", ", u.replacer->first_name, ", ", u.replacer->last_name,
							"  and  ",
								nu.username, ", ", nu.first_name, ", ", nu.last_name);
					}

					u.replacer = &nu;
					// Actualize new user data
					if (&skipped_users == &skipped_a_users) {
						nu.username = u.username;
						nu.first_name = u.first_name;
						nu.last_name = u.last_name;
						nu.email = u.email;
						nu.salt = u.salt;
						nu.password = u.password;
						nu.type = u.type;
					}
				}
			}

			if (u.replacer == nullptr) {
				// Need to add a new user
				auto& nu = new_users[++nid] = u;
				nu.id = nid;
				u.replacer = &nu;
			}

			users_map.emplace(u.id, u);
		}
	};

	read_users(a_conn, skipped_a_users, a_users);
	read_users(b_conn, skipped_b_users, b_users);

	// List merges
	stdlog("======================= Users =======================");
	for (auto&& p : new_users) {
		vector<pair<User*, bool>> maps;
		for (auto&& q : a_users)
			if (q.second.replacer == &p.second)
				maps.emplace_back(&q.second, false);
		for (auto&& q : b_users)
			if (q.second.replacer == &p.second)
				maps.emplace_back(&q.second, true);

		assert(not maps.empty());
		if (maps.size() > 1) {
			auto& nu = p.second;
			stdlog("");
			stdlog("Mappings to user ", nu.username, ", ", nu.first_name, ", ", nu.last_name, ", ", nu.email, ":");
			for (auto q : maps)
				stdlog("  ", "AB"[q.second], " ", q.first->username, ", ", q.first->first_name, ", ", q.first->last_name, ", ", q.first->email);
		}
	}

	// Insert users into the new sim
	new_conn.update("DELETE FROM users");
	auto stmt = new_conn.prepare("INSERT INTO users(id, username, first_name, last_name, email, salt, password, type) VALUES(?, ?, ?, ?, ?, ?, ?, ?)");
	User u;
	for (auto&& p : new_users) {
		u = p.second;
		stmt.bindAndExecute(u.id, u.username, u.first_name, u.last_name, u.email, u.salt, u.password, u.type);
	}
}

int64_t new_user_id(const decltype(a_users)& umap, int64_t user_id, bool try_skipped = false) {
	auto it = umap.find(user_id);
	if (it == umap.end()) {
		if (try_skipped) {
			auto& skipped = (&umap == &a_users ? skipped_a_users : skipped_b_users);
			auto it2 = skipped.find(user_id);
			if (it2 != skipped.end())
				return it2->second;
		}

		THROW("Trying to get new id of a nonexistent user (id: ", user_id, ')');
	}

	return it->second.replacer->id;
}

struct Problem {
	int64_t id;
	int type;
	InplaceBuff<40> name, label, simfile;
	int64_t owner;
	InplaceBuff<40> added, last_edit;

	Problem* replacer = nullptr; // In new_problems it points to the newest problem that maps to it
};

map<int64_t, int64_t> skipped_a_problems, skipped_b_problems;
map<int64_t, Problem> a_problems, b_problems, new_problems;

void link(FilePath old_path, FilePath new_path) {
	if (link(old_path.data(), new_path.data()))
		THROW("link('", old_path, "', '", new_path, "')", errmsg());
}

// TODO: order new problems by their added-date

// Works in O(n^2) where n is the total number of problems
void load_problems() {
	// Prepare place packages
	remove_r(concat(new_build, "/problems/"));
	mkdir(concat(new_build, "/problems/"));
	new_conn.update("DELETE FROM problems");

	// TemporaryDirectory tmp_dir("/tmp/sim_importer.XXXXXX");

	// Collect all problems
	auto read_problems = [&](auto& conn, auto& problems_map) {
		Problem p;
		auto stmt = conn.prepare("SELECT id, type, name, label, simfile, owner, added, last_edit FROM problems ORDER BY id");
		stmt.bindAndExecute();
		stmt.res_bind_all(p.id, p.type, p.name, p.label, p.simfile, p.owner, p.added, p.last_edit);

		while (stmt.next())
			problems_map.emplace(p.id, p);
	};

	read_problems(a_conn, a_problems);
	read_problems(b_conn, b_problems);

	// Adds to new_problems and hardlinks problem package (or maps to already added problem)
	auto add_to_new_problems = [&, nid = 0ll](auto& prob, auto& last_pid, auto& skipped_problems, auto&& build_dir, auto&& user_map) mutable {
		while (++last_pid < prob.id)
			skipped_problems.emplace(last_pid, ++nid);

		auto zip_to_components = [&](FilePath zipfile) {
			vector<string> entries;
			ZipFile zip(zipfile);
			for (int i = 0; i < zip.entries_no(); ++i)
				entries.emplace_back(zip.get_name(i));

			sort(entries.begin(), entries.end());
			return entries;
		};

		auto match = [&](const Problem& oldp, const Problem& newp) {
			// Compare by name
			if (oldp.name != newp.name)
				return false;

			auto old_pkg = concat(build_dir, "/problems/", oldp.id, ".zip");
			auto new_pkg = concat(new_build, "/problems/", newp.id, ".zip");

			// Compare by archive entries
			if (zip_to_components(old_pkg) != zip_to_components(new_pkg))
				return false;

			auto old_masterdir = sim::zip_package_master_dir(old_pkg);
			auto old_simfile = sim::Simfile(oldp.simfile.to_string());
			old_simfile.loadStatement();
			ZipFile old_zip(old_pkg);
			auto old_doc = old_zip.extract_to_str(old_zip.get_index(concat(old_masterdir, old_simfile.statement)));

			auto new_masterdir = sim::zip_package_master_dir(new_pkg);
			auto new_simfile = sim::Simfile(newp.simfile.to_string());
			new_simfile.loadStatement();
			ZipFile new_zip(new_pkg);
			auto new_doc = new_zip.extract_to_str(new_zip.get_index(concat(new_masterdir, new_simfile.statement)));

			// Compare by statement
			if (old_doc != new_doc)
				return false;

			return true;
		};

		// Check if there is already another problem which might be a candidate for merging
		for (auto&& i : new_problems) {
			auto& np = i.second;
			if (match(prob, np)) {
				if (prob.replacer != nullptr) {
					THROW("Problem collision was found: ",
						prob.id, ", ", prob.name, ", ", prob.label,
						" matches with ",
							prob.replacer->id, ", ", prob.replacer->name, ", ", prob.replacer->label,
						"  and  ",
							np.id, ", ", np.name, ", ", np.label);
				}

				prob.replacer = &np;
			}
		}

		if (prob.replacer == nullptr) {
			// Need to add a new problem
			auto& np = new_problems[++nid] = prob;
			np.id = nid;
			np.owner = new_user_id(user_map, np.owner);
			prob.replacer = &np;
			np.replacer = &prob;

			// Hardlink package files
			link(concat(build_dir, "/problems/", prob.id, ".zip"),
				concat(new_build, "/problems/", nid, ".zip"));

		// Actualize replacing problem
		} else {
			auto &np = *prob.replacer;
			assert(prob.added >= np.added); // We process problems in the same order they were added
			// if (prob.added < np.added) {
			// 	np.added = prob.added;
			// 	np.owner = new_user_id(user_map, prob.owner);
			// }

			// We have newer data to actualize problem
			if (prob.last_edit > prob.replacer->last_edit) {
				np.type = prob.type;
				np.name = prob.name;
				np.label = prob.label;
				np.simfile = prob.simfile;
				np.last_edit = prob.last_edit;

				np.replacer = &prob;
				// Hardlink package files
				remove(concat(new_build, "/problems/", np.id, ".zip"));
				link(concat(build_dir, "/problems/", prob.id, ".zip"),
					concat(new_build, "/problems/", np.id, ".zip"));
			}
		}
	};

	// Merge a_problems and b_problems into new_problems (order by added_date)
	int64_t last_a_pid = 0, last_b_pid = 0;
	merge(a_problems.begin(), a_problems.end(), b_problems.begin(), b_problems.end(),
		[&](auto&& p) { add_to_new_problems(p.second, last_a_pid, skipped_a_problems, a_build, a_users); },
		[&](auto&& p) { add_to_new_problems(p.second, last_b_pid, skipped_b_problems, b_build, b_users); },
		[](auto&& l, auto&& r) { return l.second.added <= r.second.added; });

	// List merges
	stdlog("======================= Problems =======================");
	for (auto&& i : new_problems) {
		vector<pair<Problem*, bool>> maps;
		for (auto&& q : a_problems)
			if (q.second.replacer == &i.second)
				maps.emplace_back(&q.second, false);
		for (auto&& q : b_problems)
			if (q.second.replacer == &i.second)
				maps.emplace_back(&q.second, true);

		assert(not maps.empty());
		if (maps.size() > 1) {
			auto& np = i.second;
			stdlog("");
			stdlog("Mappings to problem ", np.id, ", ", np.name, ", ", np.label, ":");
			for (auto q : maps)
				stdlog("  ", "AB"[q.second], " ", q.first->id, ", ", q.first->name, ", ", q.first->label);
		}
	}

	// Insert problems into the new sim
	auto stmt = new_conn.prepare("INSERT INTO problems(id, type, name, label, simfile, owner, added, last_edit) VALUES(?, ?, ?, ?, ?, ?, ?, ?)");
	Problem p;
	for (auto&& i : new_problems) {
		p = i.second;
		stmt.bindAndExecute(p.id, p.type, p.name, p.label, p.simfile, p.owner, p.added, p.last_edit);
	}
}

int64_t new_problem_id(decltype(a_problems)& pmap, int64_t problem_id, bool try_skipped = false) {
	auto it = pmap.find(problem_id);
	if (it == pmap.end()) {
		if (try_skipped) {
			auto& skipped = (&pmap == &a_problems ? skipped_a_problems : skipped_b_problems);
			auto it2 = skipped.find(problem_id);
			if (it2 != skipped.end())
				return it2->second;
		}

		THROW("Trying to get new id of a nonexistent problem (id: ", problem_id, ')');
	}

	return it->second.replacer->id;
}

void load_problem_tags() {
	auto process_ptags = [&](MySQL::Connection& conn, auto& pmap) {
		int64_t pid;
		InplaceBuff<64> tag;
		bool hidden;
		auto instmt = conn.prepare("SELECT problem_id, tag, hidden FROM problem_tags WHERE (SELECT id FROM problems WHERE id=problem_id) IS NOT NULL");
		instmt.bindAndExecute();
		instmt.res_bind_all(pid, tag, hidden);
		auto outstmt = new_conn.prepare("INSERT IGNORE INTO problem_tags(problem_id, tag, hidden) VALUES(?,?,?)");
		while (instmt.next())
			outstmt.bindAndExecute(new_problem_id(pmap, pid), tag, hidden);
	};

	new_conn.update("DELETE FROM problem_tags");
	process_ptags(a_conn, a_problems);
	process_ptags(b_conn, b_problems);
}

struct Contest {
	int64_t id;
	InplaceBuff<64> name;
	bool is_public;
	InplaceBuff<64> first_submit_time;

	Contest* replacer = nullptr;
};

map<int64_t, int64_t> skipped_a_contests, skipped_b_contests;
map<int64_t, Contest> a_contests, b_contests, new_contests;

void load_contests() {
	Contest c;
	auto process_contests = [&](MySQL::Connection& conn, auto& cmap) {
		auto stmt = conn.prepare("SELECT id, name, is_public, COALESCE((SELECT submit_time FROM submissions s WHERE contest_id=c.id ORDER BY s.id LIMIT 1),'') FROM contests c ORDER BY c.id");
		stmt.bindAndExecute();
		stmt.res_bind_all(c.id, c.name, c.is_public, c.first_submit_time);
		while (stmt.next())
			cmap.emplace(c.id, c);
	};

	process_contests(a_conn, a_contests);
	process_contests(b_conn, b_contests);

	new_conn.update("DELETE FROM contests");
	auto stmt = new_conn.prepare("INSERT INTO contests(id, name, is_public) VALUES(?,?,?)");
	auto add_contest = [&, nid = 0ll](Contest& con, auto& last_cid, auto& skipped_contests) mutable {
		while (++last_cid < con.id)
			skipped_contests.emplace(last_cid, ++nid);

		auto& ncon = new_contests[++nid] = con;
		ncon.id = nid;
		con.replacer = &ncon;

		stmt.bindAndExecute(ncon.id, ncon.name, ncon.is_public);
	};

	int64_t last_a_cid = 0, last_b_cid = 0;
	merge(a_contests.begin(), a_contests.end(), b_contests.begin(), b_contests.end(),
		[&](auto&& p) { add_contest(p.second, last_a_cid, skipped_a_contests); },
		[&](auto&& p) { add_contest(p.second, last_b_cid, skipped_b_contests); },
		[](auto&& l, auto&& r) { return l.second.first_submit_time <= r.second.first_submit_time; });
}

int64_t new_contest_id(decltype(a_contests)& cmap, int64_t contest_id, bool try_skipped = false) {
	auto it = cmap.find(contest_id);
	if (it == cmap.end()) {
		if (try_skipped) {
			auto& skipped = (&cmap == &a_contests ? skipped_a_contests : skipped_b_contests);
			auto it2 = skipped.find(contest_id);
			if (it2 != skipped.end())
				return it2->second;
		}

		THROW("Trying to get new id of a nonexistent contest (id: ", contest_id, ')');
	}

	return it->second.replacer->id;
}

struct ContestRound {
	int64_t id, contest_id;
	InplaceBuff<64> name;
	int item;
	InplaceBuff<32> begins, ends, full_results, ranking_exposure;

	ContestRound* replacer = nullptr;
};

map<int64_t, int64_t> skipped_a_contest_rounds, skipped_b_contest_rounds;
map<int64_t, ContestRound> a_contest_rounds, b_contest_rounds, new_contest_rounds;

void load_contest_rounds() {
	ContestRound cr;
	auto process_contest_rounds = [&](MySQL::Connection& conn, auto& cmap) {
		auto stmt = conn.prepare("SELECT id, contest_id, name, item, begins, ends, full_results, ranking_exposure FROM contest_rounds ORDER BY id");
		stmt.bindAndExecute();
		stmt.res_bind_all(cr.id, cr.contest_id, cr.name, cr.item, cr.begins, cr.ends, cr.full_results, cr.ranking_exposure);
		while (stmt.next())
			cmap.emplace(cr.id, cr);
	};

	process_contest_rounds(a_conn, a_contest_rounds);
	process_contest_rounds(b_conn, b_contest_rounds);

	new_conn.update("DELETE FROM contest_rounds");
	auto stmt = new_conn.prepare("INSERT INTO contest_rounds(id, contest_id, name, item, begins, ends, full_results, ranking_exposure) VALUES(?,?,?,?,?,?,?,?)");
	auto add_contest_round = [&, nid = 0ll](ContestRound& cr, auto& last_crid, auto& skipped_contest_rounds, auto& cmap) mutable {
		while (++last_crid < cr.id)
			skipped_contest_rounds.emplace(last_crid, ++nid);

		auto& ncr = new_contest_rounds[++nid] = cr;
		ncr.id = nid;
		ncr.contest_id = new_contest_id(cmap, ncr.contest_id);
		cr.replacer = &ncr;

		stmt.bindAndExecute(ncr.id, ncr.contest_id, ncr.name, ncr.item, ncr.begins, ncr.ends, ncr.full_results, ncr.ranking_exposure);
	};

	int64_t last_a_crid = 0, last_b_crid = 0;
	merge(a_contest_rounds.begin(), a_contest_rounds.end(), b_contest_rounds.begin(), b_contest_rounds.end(),
		[&](auto&& p) { add_contest_round(p.second, last_a_crid, skipped_a_contest_rounds, a_contests); },
		[&](auto&& p) { add_contest_round(p.second, last_b_crid, skipped_b_contest_rounds, b_contests); },
		[](auto&& l, auto&& r) { return l.second.begins <= r.second.begins; });
}

int64_t new_contest_round_id(decltype(a_contest_rounds)& crmap, int64_t contest_round_id, bool try_skipped = false) {
	auto it = crmap.find(contest_round_id);
	if (it == crmap.end()) {
		if (try_skipped) {
			auto& skipped = (&crmap == &a_contest_rounds ? skipped_a_contest_rounds : skipped_b_contest_rounds);
			auto it2 = skipped.find(contest_round_id);
			if (it2 != skipped.end())
				return it2->second;
		}

		THROW("Trying to get new id of a nonexistent contest_round (id: ", contest_round_id, ')');
	}

	return it->second.replacer->id;
}

struct ContestProblem {
	int64_t id, contest_round_id, contest_id, problem_id;
	InplaceBuff<64> name;
	int item, final_selecting_method, reveal_score;

	ContestProblem* replacer = nullptr;
};

map<int64_t, int64_t> skipped_a_contest_problems, skipped_b_contest_problems;
map<int64_t, ContestProblem> a_contest_problems, b_contest_problems, new_contest_problems;

void load_contest_problems() {
	ContestProblem cp;
	auto process_contest_problems = [&](MySQL::Connection& conn, auto& cmap) {
		auto stmt = conn.prepare("SELECT id, contest_round_id, contest_id, problem_id, name, item, final_selecting_method, reveal_score FROM contest_problems ORDER BY id");
		stmt.bindAndExecute();
		stmt.res_bind_all(cp.id, cp.contest_round_id, cp.contest_id, cp.problem_id, cp.name, cp.item, cp.final_selecting_method, cp.reveal_score);
		while (stmt.next())
			cmap.emplace(cp.id, cp);
	};

	process_contest_problems(a_conn, a_contest_problems);
	process_contest_problems(b_conn, b_contest_problems);

	new_conn.update("DELETE FROM contest_problems");
	auto stmt = new_conn.prepare("INSERT INTO contest_problems(id, contest_round_id, contest_id, problem_id, name, item, final_selecting_method, reveal_score) VALUES(?,?,?,?,?,?,?,?)");
	auto add_contest_problem = [&, nid = 0ll](ContestProblem& cp, auto& last_cpid, auto& skipped_contest_problems, auto& cmap, auto& crmap, auto& pmap) mutable {
		while (++last_cpid < cp.id)
			skipped_contest_problems.emplace(last_cpid, ++nid);

		auto& ncp = new_contest_problems[++nid] = cp;
		ncp.id = nid;
		ncp.contest_id = new_contest_id(cmap, ncp.contest_id);
		ncp.contest_round_id = new_contest_round_id(crmap, ncp.contest_round_id);
		ncp.problem_id = new_problem_id(pmap, ncp.problem_id);
		cp.replacer = &ncp;

		stmt.bindAndExecute(ncp.id, ncp.contest_round_id, ncp.contest_id, ncp.problem_id, ncp.name, ncp.item, ncp.final_selecting_method, ncp.reveal_score);
	};

	int64_t last_a_cpid = 0, last_b_cpid = 0;
	merge(a_contest_problems.begin(), a_contest_problems.end(), b_contest_problems.begin(), b_contest_problems.end(),
		[&](auto&& p) { add_contest_problem(p.second, last_a_cpid, skipped_a_contest_problems, a_contests, a_contest_rounds, a_problems); },
		[&](auto&& p) { add_contest_problem(p.second, last_b_cpid, skipped_b_contest_problems, b_contests, b_contest_rounds, b_problems); },
		[](auto&& l, auto&& r) { return l.second.contest_round_id <= r.second.contest_round_id; });
}

int64_t new_contest_problem_id(decltype(a_contest_problems)& cpmap, int64_t contest_problem_id, bool try_skipped = false) {
	auto it = cpmap.find(contest_problem_id);
	if (it == cpmap.end()) {
		if (try_skipped) {
			auto& skipped = (&cpmap == &a_contest_problems ? skipped_a_contest_problems : skipped_b_contest_problems);
			auto it2 = skipped.find(contest_problem_id);
			if (it2 != skipped.end())
				return it2->second;
		}

		THROW("Trying to get new id of a nonexistent contest_problem (id: ", contest_problem_id, ')');
	}

	return it->second.replacer->id;
}

void load_contest_users() {
	auto process_ptags = [&](MySQL::Connection& conn, auto& umap, auto& cmap) {
		int64_t user_id, contest_id;
		int mode;
		auto instmt = conn.prepare("SELECT user_id, contest_id, mode FROM contest_users");
		instmt.bindAndExecute();
		instmt.res_bind_all(user_id, contest_id, mode);
		auto outstmt = new_conn.prepare("INSERT IGNORE INTO contest_users(user_id, contest_id, mode) VALUES(?,?,?)"); // IGNORE because of merged accounts
		while (instmt.next())
			outstmt.bindAndExecute(new_user_id(umap, user_id), new_contest_id(cmap, contest_id), mode);
	};

	new_conn.update("DELETE FROM contest_users");
	process_ptags(a_conn, a_users, a_contests);
	process_ptags(b_conn, b_users, b_contests);
}

void load_contest_files() {
	remove_r(concat(new_build, "/files/"));
	mkdir(concat(new_build, "/files/"));
	new_conn.update("DELETE FROM files");
	auto instmt = new_conn.prepare("INSERT INTO files(id, contest_id, name, description, file_size, modified, creator) VALUES(?,?,?,?,?,?,?)");
	auto process_files = [&](MySQL::Connection& conn, auto& umap, auto& cmap, auto&& build_dir) {
		InplaceBuff<64> id;
		int64_t contest_id;
		InplaceBuff<64> name, description;
		int64_t file_size;
		InplaceBuff<32> modified;
		MySQL::Optional<int64_t> creator;

		auto stmt = conn.prepare("SELECT id, contest_id, name, description, file_size, modified, creator FROM files");
		stmt.bindAndExecute();
		stmt.res_bind_all(id, contest_id, name, description, file_size, modified, creator);
		while (stmt.next()) {
			contest_id = new_contest_id(cmap, contest_id);
			Optional<int64_t> new_creator = creator;
			if (creator.has_value())
				new_creator = new_user_id(umap, *creator);
			instmt.bindAndExecute(id, contest_id, name, description, file_size, modified, new_creator);
			link(concat(build_dir, "/files/", id), concat(new_build, "/files/", id));
		}
	};

	process_files(a_conn, a_users, a_contests, a_build);
	process_files(b_conn, b_users, b_contests, b_build);
}

void load_contest_entry_tokens() {
	new_conn.update("DELETE FROM contest_entry_tokens");
	auto instmt = new_conn.prepare("INSERT INTO contest_entry_tokens(token, contest_id, short_token, short_token_expiration) VALUES(?,?,?,?)");
	auto process_files = [&](MySQL::Connection& conn, auto& cmap) {
		InplaceBuff<64> token;
		int64_t contest_id;
		MySQL::Optional<InplaceBuff<64>> short_token, short_token_expiration;

		auto stmt = conn.prepare("SELECT token, contest_id, short_token, short_token_expiration FROM contest_entry_tokens");
		stmt.bindAndExecute();
		stmt.res_bind_all(token, contest_id, short_token, short_token_expiration);
		while (stmt.next()) {
			contest_id = new_contest_id(cmap, contest_id);
			instmt.bindAndExecute(token, contest_id, short_token, short_token_expiration);
		}
	};

	process_files(a_conn, a_contests);
	process_files(b_conn, b_contests);
}

struct Submission {
	int64_t id;
	Optional<int64_t> owner;
	int64_t problem_id;
	Optional<int64_t> contest_problem_id, contest_round_id, contest_id;
	int type, language;
	bool final_candidate, problem_final, contest_final, contest_initial_final;
	int initial_status, full_status;
	InplaceBuff<32> submit_time;
	Optional<int64_t> score;
	InplaceBuff<32> last_judgment;
	InplaceBuff<1> initial_report, final_report;

	Submission* replacer = nullptr;
};

map<int64_t, int64_t> skipped_a_submissions, skipped_b_submissions;
map<int64_t, Submission> a_submissions, b_submissions, new_submissions;

void load_submissions() {
	Submission s;
	auto process_submissions = [&](MySQL::Connection& conn, auto& cmap) {
		auto stmt = conn.prepare("SELECT id, owner, problem_id, contest_problem_id, contest_round_id, contest_id, type, language, final_candidate, problem_final, contest_final, contest_initial_final, initial_status, full_status, submit_time, score, last_judgment, initial_report, final_report FROM submissions ORDER BY id");
		stmt.bindAndExecute();

		MySQL::Optional<int64_t> owner;
		MySQL::Optional<int64_t> contest_problem_id, contest_round_id, contest_id;
		MySQL::Optional<int64_t> score;

		stmt.res_bind_all(s.id, owner, s.problem_id, contest_problem_id, contest_round_id, contest_id, s.type, s.language, s.final_candidate, s.problem_final, s.contest_final, s.contest_initial_final, s.initial_status, s.full_status, s.submit_time, score, s.last_judgment, s.initial_report, s.final_report);
		while (stmt.next()) {
			s.owner = owner;
			s.contest_problem_id = contest_problem_id;
			s.contest_round_id = contest_round_id;
			s.contest_id = contest_id;
			s.score = score;
			cmap.emplace(s.id, s);
		}
	};

	process_submissions(a_conn, a_submissions);
	process_submissions(b_conn, b_submissions);

	new_conn.update("DELETE FROM submissions");
	remove_r(concat(new_build, "/solutions/"));
	mkdir(concat(new_build, "/solutions/"));
	auto stmt = new_conn.prepare("INSERT INTO submissions(id, owner, problem_id, contest_problem_id, contest_round_id, contest_id, type, language, final_candidate, problem_final, contest_final, contest_initial_final, initial_status, full_status, submit_time, score, last_judgment, initial_report, final_report) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
	AVLDictSet<array<InplaceBuff<6>, 3>> finals_to_update;

	auto add_submission = [&, nid = 0ll](Submission& s, auto& last_sid, auto& skipped_submissions, auto& umap, auto& cmap, auto& crmap, auto& cpmap, auto& pmap, auto&& old_build) mutable {

		while (++last_sid < s.id)
			skipped_submissions.emplace(last_sid, ++nid);

		// To avoid model solution duplications, we import solutions of the newest package of the problem
		if (EnumVal<SubmissionType>(s.type) == SubmissionType::PROBLEM_SOLUTION and &pmap[s.problem_id] != pmap[s.problem_id].replacer->replacer) {
			skipped_submissions.emplace(last_sid, ++nid);
			return;
		}

		auto& ns = new_submissions[++nid] = s;
		ns.id = nid;
		if (ns.owner.has_value())
			ns.owner = new_user_id(umap, *ns.owner);
		ns.problem_id = new_problem_id(pmap, ns.problem_id);
		if (ns.contest_id.has_value())
			ns.contest_id = new_contest_id(cmap, *ns.contest_id);
		if (ns.contest_round_id.has_value())
			ns.contest_round_id = new_contest_round_id(crmap, *ns.contest_round_id);
		if (ns.contest_problem_id.has_value())
			ns.contest_problem_id = new_contest_problem_id(cpmap, *ns.contest_problem_id);
		s.replacer = &ns;

		stmt.bindAndExecute(ns.id, ns.owner, ns.problem_id, ns.contest_problem_id, ns.contest_round_id, ns.contest_id, ns.type, ns.language, ns.final_candidate, ns.problem_final, ns.contest_final, ns.contest_initial_final, ns.initial_status, ns.full_status, ns.submit_time, ns.score, ns.last_judgment, ns.initial_report, ns.final_report);

		if ((ns.id & 255) == 0)
			stdlog("Submissions ", ns.id);

		link(concat(old_build, "/solutions/", s.id), concat(new_build, "/solutions/", nid));

		array<InplaceBuff<6>, 3> ftu_elem;
		if (ns.owner.has_value())
			ftu_elem[0] = toStr(*ns.owner);
		ftu_elem[1] = toStr(ns.problem_id);
		if (ns.contest_problem_id.has_value())
			ftu_elem[2] = toStr(ns.contest_problem_id.value());

		finals_to_update.emplace(ftu_elem);
	};

	int64_t last_a_sid = 0, last_b_sid = 0;
	merge(a_submissions.begin(), a_submissions.end(), b_submissions.begin(), b_submissions.end(),
		[&](auto&& p) { add_submission(p.second, last_a_sid, skipped_a_submissions, a_users, a_contests, a_contest_rounds, a_contest_problems, a_problems, a_build); },
		[&](auto&& p) { add_submission(p.second, last_b_sid, skipped_b_submissions, b_users, b_contests, b_contest_rounds, b_contest_problems, b_problems, b_build); },
		[](auto&& l, auto&& r) { return l.second.submit_time <= r.second.submit_time; });

	stdlog("Updating finals (", finals_to_update.size(), " to go)");
	finals_to_update.for_each([&, i = 0](auto&& ftu_elem) mutable {
		submission::update_final(new_conn, ftu_elem[0], ftu_elem[1], ftu_elem[2]);
		if ((++i & 255) == 0)
			stdlog("Updated ", i);
	});
}

int64_t new_submission_id(decltype(a_submissions)& smap, int64_t submission_id, bool try_skipped = false) {
	auto it = smap.find(submission_id);
	if (it == smap.end() or it->second.replacer == nullptr) { // replacer == nullptr means that solution submission was ignored while merging submissions
		if (try_skipped) {
			auto& skipped = (&smap == &a_submissions ? skipped_a_submissions : skipped_b_submissions);
			auto it2 = skipped.find(submission_id);
			if (it2 != skipped.end())
				return it2->second;
		}

		THROW("Trying to get new id of a nonexistent submission (id: ", submission_id, ')');
	}

	return it->second.replacer->id;
}

struct Job {
	int64_t id;
	Optional<int64_t> creator;
	int type, priority, status;
	InplaceBuff<32> added;
	Optional<int64_t> aux_id;
	InplaceBuff<64> info;
	InplaceBuff<32> data;
};

void load_jobs() {
	deque<Job> a_jobs, b_jobs;
	auto process_jobs = [&](MySQL::Connection& conn, auto& jdeq) {
		auto stmt = conn.prepare("SELECT id, creator, type, priority, status, added, aux_id, info, data FROM jobs ORDER BY id");
		stmt.bindAndExecute();

		Job j;
		MySQL::Optional<int64_t> creator, aux_id;

		stmt.res_bind_all(j.id, creator, j.type, j.priority, j.status, j.added, aux_id, j.info, j.data);
		while (stmt.next()) {
			j.creator = creator;
			j.aux_id = aux_id;
			jdeq.emplace_back(j);
		}
	};

	process_jobs(a_conn, a_jobs);
	process_jobs(b_conn, b_jobs);

	new_conn.update("DELETE FROM jobs");
	remove_r(concat(new_build, "/jobs_files/"));
	mkdir(concat(new_build, "/jobs_files/"));
	auto stmt = new_conn.prepare("INSERT INTO jobs(id, creator, type, priority, status, added, aux_id, info, data) VALUES(?,?,?,?,?,?,?,?,?)");
	auto add_job = [&, next_id=0ll](Job& j, auto& umap, auto& cmap, auto& crmap, auto& cpmap, auto& pmap, auto& smap, auto&& old_build) mutable {
		Job nj = j;
		nj.id = ++next_id;
		if (nj.creator.has_value())
			nj.creator = new_user_id(umap, *nj.creator, true);

		switch (JobType(nj.type)) {
		case JobType::VOID: assert(false);

		case JobType::JUDGE_SUBMISSION: {
			int64_t sid = nj.aux_id.value();
			int64_t pid = strtoull(intentionalUnsafeStringView(jobs::extractDumpedString(nj.info)));
			sid = new_submission_id(smap, sid, true);
			pid = new_problem_id(pmap, pid, true);

			nj.aux_id = sid;
			nj.info = jobs::dumpString(intentionalUnsafeStringView(toStr(pid)));
			break;
		}

		case JobType::ADD_PROBLEM:
		case JobType::REUPLOAD_PROBLEM:
		case JobType::ADD_JUDGE_MODEL_SOLUTION:
		case JobType::REUPLOAD_JUDGE_MODEL_SOLUTION:
		case JobType::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
		case JobType::EDIT_PROBLEM:
		case JobType::DELETE_PROBLEM: {
			if (nj.aux_id.has_value())
				nj.aux_id = new_problem_id(pmap, nj.aux_id.value(), true);
			break;
		}

		case JobType::DELETE_USER: {
			nj.aux_id = new_user_id(umap, nj.aux_id.value(), true);
			break;
		}

		case JobType::CONTEST_PROBLEM_RESELECT_FINAL_SUBMISSIONS: {
			nj.aux_id = new_contest_problem_id(cpmap, nj.aux_id.value(), true);
			break;
		}

		case JobType::DELETE_CONTEST: {
			nj.aux_id = new_contest_id(cmap, nj.aux_id.value(), true);
			break;
		}

		case JobType::DELETE_CONTEST_ROUND: {
			nj.aux_id = new_contest_round_id(crmap, nj.aux_id.value(), true);
			break;
		}

		case JobType::DELETE_CONTEST_PROBLEM: {
			nj.aux_id = new_contest_problem_id(cpmap, nj.aux_id.value(), true);
			break;
		}
		}

		stmt.bindAndExecute(nj.id, nj.creator, nj.type, nj.priority, nj.status, nj.added, nj.aux_id, nj.info, nj.data);

		auto old_job_zip = concat(old_build, "/jobs_files/", j.id, ".zip");
		if (access(old_job_zip, F_OK) == 0)
			link(old_job_zip, concat(new_build, "/jobs_files/", nj.id, ".zip"));
	};

	merge(a_jobs.begin(), a_jobs.end(), b_jobs.begin(), b_jobs.end(),
		[&](auto&& x) { add_job(x, a_users, a_contests, a_contest_rounds, a_contest_problems, a_problems, a_submissions, a_build); },
		[&](auto&& x) { add_job(x, b_users, b_contests, b_contest_rounds, b_contest_problems, b_problems, b_submissions, b_build); },
		[](auto&& l, auto&& r) { return l.added <= r.added; });
}

void insert_problem_time_limits_reseting_jobs() {
	auto stmt = new_conn.prepare("INSERT INTO jobs(creator, status, priority, type, added, aux_id, info, data) VALUES(NULL, " JSTATUS_PENDING_STR ", ?, " JTYPE_RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION_STR ", ?, ?, '', '')");
	for (auto&& p : new_problems) {
		auto pid = p.first;
		stmt.bindAndExecute(priority(JobType::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION), mysql_date(), pid);
	}
}

int main2(int argc, char **argv) {
	STACK_UNWINDING_MARK;

	stdlog.use(stdout);

	if (argc != 4) {
		errlog.label(false);
		errlog("Use: importer <sim1_build> <sim2_build> <sim_result_build> where sim1_build and sim2_build are paths to merged sims build/ directories and sim_result_build is a path to fresh installed sim where the result will be placed.\nAll big files will be hard linked instead of copies, overwriting old files will overwrite the new ones");
		return 1;
	}

	try {
		// Get connection
		a_conn = MySQL::make_conn_with_credential_file(
			concat_tostr(a_build = argv[1], "/.db.config"));
		b_conn = MySQL::make_conn_with_credential_file(
			concat_tostr(b_build = argv[2], "/.db.config"));
		new_conn = MySQL::make_conn_with_credential_file(
			concat_tostr(new_build = argv[3], "/.db.config"));
		// importer_db = SQLite::Connection("importer.db",
			// SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
	} catch (const std::exception& e) {
		errlog("\033[31mFailed to connect to databases\033[m - ", e.what());
		return 4;
	}

	// a_conn.update("SET character_set_results=NULL"); // Disable stupid conversions
	// b_conn.update("SET character_set_results=NULL"); // Disable stupid conversions
	// new_conn.update("SET character_set_results=NULL"); // Disable stupid conversions

	// Kill both webservers and job-servers to not interrupt with the merging
	auto killinstc = concat_tostr(getExecDir(getpid()), "/../src/killinstc");
	Spawner::run(killinstc, {
		killinstc,
		concat_tostr(a_build, "/sim-server"),
		concat_tostr(a_build, "/job-server"),
		concat_tostr(b_build, "/sim-server"),
		concat_tostr(b_build, "/job-server"),
		concat_tostr(new_build, "/sim-server"),
		concat_tostr(new_build, "/job-server"),
	});

	// Users
	load_users();

	// Session is ignored
	// Problems
	load_problems();

	// Problem tags
	load_problem_tags();

	// Contests
	load_contests();

	// Contest rounds
	load_contest_rounds();

	// Contest problems
	load_contest_problems();

	// Contest users
	load_contest_users();

	// Contest files
	load_contest_files();

	// Contest entry tokens
	load_contest_entry_tokens();

	// Submissions
	load_submissions();

	// Jobs
	load_jobs();

	// Reset problem time limits
	insert_problem_time_limits_reseting_jobs();

	return 0;
}

namespace old_jobs {

/// Append an integer @p x in binary format to the @p buff
template<class Integer>
inline std::enable_if_t<std::is_integral<Integer>::value, void>
	appendDumpedInt(std::string& buff, Integer x)
{
	buff.append(sizeof(x), '\0');
	for (uint i = 1, shift = 0; i <= sizeof(x); ++i, shift += 8)
		buff[buff.size() - i] = (x >> shift) & 0xFF;
}

template<class Integer>
inline std::enable_if_t<std::is_integral<Integer>::value, Integer>
	extractDumpedInt(StringView& dumped_str)
{
	throw_assert(dumped_str.size() >= sizeof(Integer));
	Integer x = 0;
	for (int i = sizeof(x) - 1, shift = 0; i >= 0; --i, shift += 8)
		x |= ((static_cast<Integer>((uint8_t)dumped_str[i])) << shift);
	dumped_str.removePrefix(sizeof(x));
	return x;
}

template<class Integer>
inline std::enable_if_t<std::is_integral<Integer>::value, void>
	extractDumpedInt(Integer& x, StringView& dumped_str)
{
	x = extractDumpedInt<Integer>(dumped_str);
}

/// Dumps @p str to binary format XXXXABC... where XXXX code string's size and
/// ABC... is the @p str and appends it to the @p buff
inline void appendDumpedString(std::string& buff, StringView str) {
	appendDumpedInt<uint32_t>(buff, str.size());
	buff += str;
}

/// Returns dumped @p str to binary format XXXXABC... where XXXX code string's
/// size and ABC... is the @p str
inline std::string dumpString(StringView str) {
	std::string res;
	appendDumpedString(res, str);
	return res;
}

inline std::string extractDumpedString(StringView& dumped_str) {
	uint32_t size;
	extractDumpedInt(size, dumped_str);
	throw_assert(dumped_str.size() >= size);
	return dumped_str.extractPrefix(size).to_string();
}

inline std::string extractDumpedString(StringView&& dumped_str) {
	return extractDumpedString(dumped_str); /* std::move() is intentionally
		omitted in order to call the above implementation */
}

struct AddProblemInfo {
	std::string name, label;
	uint64_t memory_limit = 0; // in bytes
	uint64_t global_time_limit = 0; // in usec
	bool reset_time_limits = false;
	bool ignore_simfile = false;
	bool seek_for_new_tests = false;
	bool reset_scoring = false;
	ProblemType problem_type = ProblemType::VOID;
	enum Stage : uint8_t { FIRST = 0, SECOND = 1 } stage = FIRST;

	AddProblemInfo() = default;

	AddProblemInfo(const std::string& n, const std::string& l, uint64_t ml,
			uint64_t gtl, bool rtl, bool is, bool sfnt, bool rs, ProblemType pt)
		: name(n), label(l), memory_limit(ml), global_time_limit(gtl),
			reset_time_limits(rtl), ignore_simfile(is),
			seek_for_new_tests(sfnt), reset_scoring(rs), problem_type(pt) {}

	AddProblemInfo(StringView str) {
		name = extractDumpedString(str);
		label = extractDumpedString(str);
		extractDumpedInt(memory_limit, str);
		extractDumpedInt(global_time_limit, str);

		uint8_t mask = extractDumpedInt<uint8_t>(str);
		reset_time_limits = (mask & 1);
		ignore_simfile = (mask & 2);
		seek_for_new_tests = (mask & 4);
		reset_scoring = (mask & 8);

		problem_type = static_cast<ProblemType>(
			extractDumpedInt<std::underlying_type_t<ProblemType>>(str));
		stage = static_cast<Stage>(
			extractDumpedInt<std::underlying_type_t<Stage>>(str));
	}

	std::string dump() const {
		std::string res;
		appendDumpedString(res, name);
		appendDumpedString(res, label);
		appendDumpedInt(res, memory_limit);
		appendDumpedInt(res, global_time_limit);

		uint8_t mask = reset_time_limits | (int(ignore_simfile) << 1) |
			(int(seek_for_new_tests) << 2) | (int(reset_scoring) << 3);
		appendDumpedInt(res, mask);

		appendDumpedInt(res, std::underlying_type_t<ProblemType>(problem_type));
		appendDumpedInt<std::underlying_type_t<Stage>>(res, stage);
		return res;
	}
};

struct MergeProblemsInfo {
	uint64_t target_problem_id;
	bool rejudge_transferred_submissions;

	MergeProblemsInfo() = default;

	MergeProblemsInfo(uint64_t tpid, bool rts) noexcept : target_problem_id(tpid), rejudge_transferred_submissions(rts) {}

	MergeProblemsInfo(StringView str) {
		extractDumpedInt(target_problem_id, str);

		auto mask = extractDumpedInt<uint8_t>(str);
		rejudge_transferred_submissions = (mask & 1);
	}

	std::string dump() {
		std::string res;
		appendDumpedInt(res, target_problem_id);

		uint8_t mask = rejudge_transferred_submissions;
		appendDumpedInt(res, mask);
		return res;
	}
};

void restart_job(MySQL::Connection& mysql, StringView job_id, JobType job_type,
	StringView job_info, bool notify_job_server);

void restart_job(MySQL::Connection& mysql, StringView job_id,
	bool notify_job_server);

// Notifies the Job server that there are jobs to do
inline void notify_job_server() noexcept {
	utime(JOB_SERVER_NOTIFYING_FILE, nullptr);
}

} // namespace old_jobs

int main3(int argc, char **argv) {
	STACK_UNWINDING_MARK;

	stdlog.use(stdout);

	if (argc != 2) {
		errlog.label(false);
		errlog("Use: importer <sim_build> where sim_build is a path to sim");
		return 1;
	}

	try {
		// Get connection
		new_conn = MySQL::make_conn_with_credential_file(
			concat_tostr(new_build = argv[1], "/.db.config"));
		// importer_db = SQLite::Connection("importer.db",
			// SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
	} catch (const std::exception& e) {
		errlog("\033[31mFailed to connect to databases\033[m - ", e.what());
		return 4;
	}

	// a_conn.update("SET character_set_results=NULL"); // Disable stupid conversions
	// b_conn.update("SET character_set_results=NULL"); // Disable stupid conversions
	// new_conn.update("SET character_set_results=NULL"); // Disable stupid conversions

	// Kill both webservers and job-servers to not interrupt with the merging
	auto killinstc = concat_tostr(getExecDir(getpid()), "/../src/killinstc");
	Spawner::run(killinstc, {
		killinstc,
		concat_tostr(new_build, "/sim-server"),
		concat_tostr(new_build, "/job-server"),
	});

	// Upgrade jobs
	auto in_stmt = new_conn.prepare("SELECT id, info FROM jobs WHERE type IN (" JTYPE_ADD_PROBLEM_STR "," JTYPE_REUPLOAD_PROBLEM_STR "," JTYPE_ADD_JUDGE_MODEL_SOLUTION_STR "," JTYPE_REUPLOAD_JUDGE_MODEL_SOLUTION_STR ")");
	in_stmt.bindAndExecute();

	InplaceBuff<64> id, info;
	in_stmt.res_bind_all(id, info);

	auto out_stmt = new_conn.prepare("UPDATE jobs SET info=? WHERE id=?");
	while (in_stmt.next()) {
		old_jobs::AddProblemInfo old_apinfo(info);
		jobs::AddProblemInfo new_apinfo;

		new_apinfo.name = old_apinfo.name;
		new_apinfo.label = old_apinfo.label;

		if (old_apinfo.memory_limit != 0)
			new_apinfo.memory_limit = old_apinfo.memory_limit;

		if (old_apinfo.global_time_limit != 0)
			new_apinfo.global_time_limit = old_apinfo.global_time_limit;

		new_apinfo.reset_time_limits = old_apinfo.reset_time_limits;
		new_apinfo.ignore_simfile = old_apinfo.ignore_simfile;
		new_apinfo.seek_for_new_tests = old_apinfo.seek_for_new_tests;
		new_apinfo.reset_scoring = old_apinfo.reset_scoring;
		new_apinfo.problem_type = old_apinfo.problem_type;
		new_apinfo.stage = EnumVal<jobs::AddProblemInfo::Stage>(old_apinfo.stage);

		out_stmt.bindAndExecute(new_apinfo.dump(), id);
	}

	return 0;
}

int main(int argc, char **argv) {
	// try {
		return main3(argc, argv);

	// } catch (const std::exception& e) {
		// ERRLOG_CATCH(e);
		// return 1;
	// }
}

#pragma once

#include "merger.hh"

#include <sim/constants.h>
#include <sim/submission.h>
#include <sim/user.hh>

// Merges users by username
class UsersMerger : public Merger<sim::User> {
	std::map<decltype(sim::User::username), size_t> username_to_new_table_idx_;

	void load(RecordSet& record_set) override {
		STACK_UNWINDING_MARK;
		// Prefer main users to come first
		auto time = [&] {
			switch (record_set.kind) {
			case IdKind::Main: return std::chrono::system_clock::from_time_t(0);
			case IdKind::Other:
				return std::chrono::system_clock::time_point::max();
			}

			THROW("BUG");
		}();

		sim::User user;
		auto stmt = conn.prepare("SELECT id, username, first_name, last_name, "
		                         "email, salt, password, type FROM ",
		                         record_set.sql_table_name);
		stmt.bind_and_execute();
		stmt.res_bind_all(user.id, user.username, user.first_name,
		                  user.last_name, user.email, user.salt, user.password,
		                  user.type);

		while (stmt.next())
			record_set.add_record(user, time);
	}

	void merge() override {
		STACK_UNWINDING_MARK;
		Merger::merge([&](const sim::User& x, IdKind kind) -> NewRecord* {
			auto it = username_to_new_table_idx_.find(x.username);
			if (it != username_to_new_table_idx_.end()) {
				NewRecord& res = new_table_[it->second];
				stdlog("\nMerging: ", x.username, '\t', x.first_name, '\t',
				       x.last_name, '\t', x.email, ' ', id_info(x, kind),
				       "\n   into: ", res.data.username, '\t',
				       res.data.first_name, '\t', res.data.last_name, '\t',
				       res.data.email, ' ', id_info(res));
				return &res;
			}

			username_to_new_table_idx_[x.username] = new_table_.size();
			return nullptr;
		});
	}

public:
	void save_merged() override {
		STACK_UNWINDING_MARK;
		auto transaction = conn.start_transaction();
		conn.update("TRUNCATE ", sql_table_name());
		auto stmt = conn.prepare("INSERT INTO ", sql_table_name(),
		                         "(id, username, first_name, last_name, email,"
		                         " salt, password, type) "
		                         "VALUES(?, ?, ?, ?, ?, ?, ?, ?)");

		ProgressBar progress_bar("Users saved:", new_table_.size(), 128);
		for (const NewRecord& new_record : new_table_) {
			Defer progressor = [&] { progress_bar.iter(); };
			const sim::User& x = new_record.data;
			stmt.bind_and_execute(x.id, x.username, x.first_name, x.last_name,
			                      x.email, x.salt, x.password, x.type);
		}

		conn.update("ALTER TABLE ", sql_table_name(),
		            " AUTO_INCREMENT=", last_new_id_ + 1);
		transaction.commit();
	}

	UsersMerger(const IdsFromMainAndOtherJobs& ids_from_both_jobs)
	   : Merger("users", ids_from_both_jobs.main.users,
	            ids_from_both_jobs.other.users) {
		STACK_UNWINDING_MARK;
		initialize();
	}

	void run_after_saving_hooks() override {
		// Reselect final submissions for merged users originating from single
		// source
		auto transaction = conn.start_transaction();
		for (auto const& user : new_table_) {
			if (user.main_ids.size() > 1 or user.other_ids.size() > 1) {
				uintmax_t problem_id;
				MySQL::Optional<uintmax_t> contest_problem_id;

				auto stmt =
				   conn.prepare("SELECT problem_id, contest_problem_id "
				                "FROM submissions "
				                "WHERE owner=? "
				                "GROUP BY problem_id, contest_problem_id");
				stmt.bind_and_execute(user.data.id);
				stmt.res_bind_all(problem_id, contest_problem_id);
				while (stmt.next()) {
					submission::update_final(conn, user.data.id, problem_id,
					                         contest_problem_id, false);
				}
			}
		}
		// Final submissions for problems do not have to be reselect, because
		// problem are not merged during sim-merge -- they are merged using
		// MERGE_PROBLEMS jobs and there final submissions are reselected.

		transaction.commit();
	}
};

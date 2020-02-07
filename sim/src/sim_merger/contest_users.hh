#pragma once

#include "contests.hh"
#include "users.hh"

#include <sim/contest_user.hh>

class ContestUsersMerger : public Merger<sim::ContestUser> {
	const UsersMerger& users_;
	const ContestsMerger& contests_;

	void load(RecordSet& record_set) override {
		STACK_UNWINDING_MARK;

		sim::ContestUser cu;
		auto stmt = conn.prepare("SELECT user_id, contest_id, mode FROM ",
		                         record_set.sql_table_name);
		stmt.bindAndExecute();
		stmt.res_bind_all(cu.id.user_id, cu.id.contest_id, cu.mode);
		while (stmt.next()) {
			cu.id.user_id = users_.new_id(cu.id.user_id, record_set.kind);
			cu.id.contest_id =
			   contests_.new_id(cu.id.contest_id, record_set.kind);
			// Time does not matter
			record_set.add_record(cu,
			                      std::chrono::system_clock::time_point::min());
		}
	}

	void merge() override {
		STACK_UNWINDING_MARK;
		Merger::merge([&](const sim::ContestUser&) { return nullptr; });
	}

	sim::ContestUser::Id new_id_for_record_to_merge_into_new_records(
	   const sim::ContestUser::Id& record_id) override {
		return record_id;
	}

public:
	void save_merged() override {
		STACK_UNWINDING_MARK;
		auto transaction = conn.start_transaction();
		conn.update("TRUNCATE ", sql_table_name());
		auto stmt = conn.prepare("INSERT INTO ", sql_table_name(),
		                         "(user_id, contest_id, mode) "
		                         "VALUES(?, ?, ?)");

		ProgressBar progress_bar("Contest users saved:", new_table_.size(), 128);
		for (const NewRecord& new_record : new_table_) {
			Defer progressor = [&] { progress_bar.iter(); };
			const sim::ContestUser& x = new_record.data;
			stmt.bindAndExecute(x.id.user_id, x.id.contest_id, x.mode);
		}

		transaction.commit();
	}

	ContestUsersMerger(const IdsFromMainAndOtherJobs& ids_from_both_jobs,
	                   const UsersMerger& users, const ContestsMerger& contests)
	   : Merger("contest_users", ids_from_both_jobs.main.contest_users,
	            ids_from_both_jobs.other.contest_users),
	     users_(users), contests_(contests) {
		STACK_UNWINDING_MARK;
		initialize();
	}
};

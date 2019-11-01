#pragma once

#include "merger.hh"

#include <sim/constants.h>

struct Contest {
	uintmax_t id;
	InplaceBuff<CONTEST_NAME_MAX_LEN> name;
	bool is_public;
};

class ContestsMerger : public Merger<Contest> {
	void load(RecordSet& record_set) override {
		STACK_UNWINDING_MARK;

		Contest c;
		MySQL::Optional<InplaceBuff<24>> earliest_submit_time;
		unsigned char b_is_public;
		auto stmt =
		   conn.prepare("SELECT c.id, c.name, c.is_public, MIN(s.submit_time) "
		                "FROM ",
		                record_set.sql_table_name, " c LEFT JOIN ",
		                record_set.sql_table_prefix,
		                "submissions s "
		                "ON s.contest_id=c.id GROUP BY c.id");
		stmt.bindAndExecute();
		stmt.res_bind_all(c.id, c.name, b_is_public, earliest_submit_time);

		auto curr_time = std::chrono::system_clock::now();
		while (stmt.next()) {
			c.is_public = b_is_public;
			auto time = (earliest_submit_time.has_value()
			                ? strToTimePoint(earliest_submit_time->to_string())
			                : curr_time);
			record_set.add_record(c, time);
		}
	}

	void merge() override {
		STACK_UNWINDING_MARK;
		Merger::merge([&](const Contest&) { return nullptr; });
	}

public:
	void save_merged() override {
		STACK_UNWINDING_MARK;
		auto transaction = conn.start_transaction();
		conn.update("TRUNCATE ", sql_table_name());
		auto stmt = conn.prepare("INSERT INTO ", sql_table_name(),
		                         "(id, name, is_public) VALUES(?, ?, ?)");
		for (const NewRecord& new_record : new_table_) {
			const Contest& x = new_record.data;
			stmt.bindAndExecute(x.id, x.name, x.is_public);
		}

		conn.update("ALTER TABLE ", sql_table_name(),
		            " AUTO_INCREMENT=", last_new_id_ + 1);
		transaction.commit();
	}

	ContestsMerger(const IdsFromMainAndOtherJobs& ids_from_both_jobs)
	   : Merger("contests", ids_from_both_jobs.main.contests,
	            ids_from_both_jobs.other.contests) {
		STACK_UNWINDING_MARK;
		initialize();
	}
};

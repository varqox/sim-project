#pragma once

#include "contests.hh"

struct ContestRound {
	uintmax_t id;
	uintmax_t contest_id;
	InplaceBuff<CONTEST_ROUND_NAME_MAX_LEN> name;
	uintmax_t item;
	InplaceBuff<CONTEST_ROUND_DATETIME_LEN> begins;
	InplaceBuff<CONTEST_ROUND_DATETIME_LEN> ends;
	InplaceBuff<CONTEST_ROUND_DATETIME_LEN> full_results;
	InplaceBuff<CONTEST_ROUND_DATETIME_LEN> ranking_exposure;
};

class ContestRoundsMerger : public Merger<ContestRound> {
	const ContestsMerger& contests_;

	void load(RecordSet& record_set) override {
		STACK_UNWINDING_MARK;

		ContestRound cr;
		MySQL::Optional<InplaceBuff<24>> earliest_submit_time;
		auto stmt =
		   conn.prepare("SELECT cr.id, cr.contest_id, cr.name, cr.item,"
		                " cr.begins, cr.ends, cr.full_results,"
		                " cr.ranking_exposure, MIN(s.submit_time) "
		                "FROM ",
		                record_set.sql_table_name, " cr LEFT JOIN ",
		                record_set.sql_table_prefix,
		                "submissions s "
		                "ON s.contest_round_id=cr.id GROUP BY cr.id");
		stmt.bindAndExecute();
		stmt.res_bind_all(cr.id, cr.contest_id, cr.name, cr.item, cr.begins,
		                  cr.ends, cr.full_results, cr.ranking_exposure,
		                  earliest_submit_time);
		auto curr_time = std::chrono::system_clock::now();
		while (stmt.next()) {
			cr.contest_id = contests_.new_id(cr.contest_id, record_set.kind);

			auto time = (earliest_submit_time.has_value()
			                ? strToTimePoint(earliest_submit_time->to_string())
			                : curr_time);
			record_set.add_record(cr, time);
		}
	}

	void merge() override {
		STACK_UNWINDING_MARK;
		Merger::merge([&](const ContestRound&) { return nullptr; });
	}

public:
	void save_merged() override {
		STACK_UNWINDING_MARK;
		auto transaction = conn.start_transaction();
		conn.update("TRUNCATE ", sql_table_name());
		auto stmt = conn.prepare("INSERT INTO ", sql_table_name(),
		                         "(id, contest_id, name, item, begins, ends,"
		                         " full_results, ranking_exposure) "
		                         "VALUES(?, ?, ?, ?, ?, ?, ?, ?)");
		for (const NewRecord& new_record : new_table_) {
			const ContestRound& x = new_record.data;
			stmt.bindAndExecute(x.id, x.contest_id, x.name, x.item, x.begins,
			                    x.ends, x.full_results, x.ranking_exposure);
		}

		conn.update("ALTER TABLE ", sql_table_name(),
		            " AUTO_INCREMENT=", last_new_id_ + 1);
		transaction.commit();
	}

	ContestRoundsMerger(const IdsFromMainAndOtherJobs& ids_from_both_jobs,
	                    const ContestsMerger& contests)
	   : Merger("contest_rounds", ids_from_both_jobs.main.contest_rounds,
	            ids_from_both_jobs.other.contest_rounds),
	     contests_(contests) {
		STACK_UNWINDING_MARK;
		initialize();
	}
};

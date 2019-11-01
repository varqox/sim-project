#pragma once

#include "contest_rounds.hh"
#include "problems.hh"

struct ContestProblem {
	uintmax_t id;
	uintmax_t contest_round_id;
	uintmax_t contest_id;
	uintmax_t problem_id;
	InplaceBuff<CONTEST_PROBLEM_NAME_MAX_LEN> name;
	uintmax_t item;
	EnumVal<SubmissionFinalSelectingMethod> final_selecting_method;
	EnumVal<ScoreRevealingMode> score_revealing;
};

class ContestProblemsMerger : public Merger<ContestProblem> {
	const ContestRoundsMerger& contest_rounds_;
	const ContestsMerger& contests_;
	const ProblemsMerger& problems_;

	void load(RecordSet& record_set) override {
		STACK_UNWINDING_MARK;

		ContestProblem cp;
		MySQL::Optional<InplaceBuff<24>> earliest_submit_time;
		auto stmt =
		   conn.prepare("SELECT cp.id, cp.contest_round_id, cp.contest_id,"
		                " cp.problem_id, cp.name, cp.item,"
		                " cp.final_selecting_method, cp.score_revealing,"
		                " MIN(s.submit_time) "
		                "FROM ",
		                record_set.sql_table_name, " cp LEFT JOIN ",
		                record_set.sql_table_prefix,
		                "submissions s "
		                "ON s.contest_problem_id=cp.id GROUP BY cp.id");
		stmt.bindAndExecute();
		stmt.res_bind_all(cp.id, cp.contest_round_id, cp.contest_id,
		                  cp.problem_id, cp.name, cp.item,
		                  cp.final_selecting_method, cp.score_revealing,
		                  earliest_submit_time);

		auto curr_time = std::chrono::system_clock::now();
		while (stmt.next()) {
			cp.contest_round_id =
			   contest_rounds_.new_id(cp.contest_round_id, record_set.kind);
			cp.contest_id = contests_.new_id(cp.contest_id, record_set.kind);
			cp.problem_id = problems_.new_id(cp.problem_id, record_set.kind);

			auto time = (earliest_submit_time.has_value()
			                ? strToTimePoint(earliest_submit_time->to_string())
			                : curr_time);
			record_set.add_record(cp, time);
		}
	}

	void merge() override {
		STACK_UNWINDING_MARK;
		Merger::merge([&](const ContestProblem&) { return nullptr; });
	}

public:
	void save_merged() override {
		STACK_UNWINDING_MARK;
		auto transaction = conn.start_transaction();
		conn.update("TRUNCATE ", sql_table_name());
		auto stmt = conn.prepare("INSERT INTO ", sql_table_name(),
		                         "(id, contest_round_id, contest_id,"
		                         " problem_id, name, item,"
		                         " final_selecting_method, score_revealing) "
		                         "VALUES(?, ?, ?, ?, ?, ?, ?, ?)");
		for (const NewRecord& new_record : new_table_) {
			const ContestProblem& x = new_record.data;
			stmt.bindAndExecute(x.id, x.contest_round_id, x.contest_id,
			                    x.problem_id, x.name, x.item,
			                    x.final_selecting_method, x.score_revealing);
		}

		conn.update("ALTER TABLE ", sql_table_name(),
		            " AUTO_INCREMENT=", last_new_id_ + 1);
		transaction.commit();
	}

	ContestProblemsMerger(const IdsFromMainAndOtherJobs& ids_from_both_jobs,
	                      const ContestRoundsMerger& contest_rounds,
	                      const ContestsMerger& contests,
	                      const ProblemsMerger& problems)
	   : Merger("contest_problems", ids_from_both_jobs.main.contest_problems,
	            ids_from_both_jobs.other.contest_problems),
	     contest_rounds_(contest_rounds), contests_(contests),
	     problems_(problems) {
		STACK_UNWINDING_MARK;
		initialize();
	}
};

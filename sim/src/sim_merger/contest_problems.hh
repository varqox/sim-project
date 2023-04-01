#pragma once

#include "contest_rounds.hh"
#include "problems.hh"

#include <sim/contest_problems/contest_problem.hh>
#include <sim/sql_fields/datetime.hh>

namespace sim_merger {

class ContestProblemsMerger : public Merger<sim::contest_problems::ContestProblem> {
    const ContestRoundsMerger& contest_rounds_;
    const ContestsMerger& contests_;
    const ProblemsMerger& problems_;

    void load(RecordSet& record_set) override {
        STACK_UNWINDING_MARK;

        sim::contest_problems::ContestProblem cp;
        mysql::Optional<sim::sql_fields::Datetime> earliest_submit_time;
        auto stmt = conn.prepare(
            "SELECT cp.id, cp.contest_round_id, cp.contest_id,"
            " cp.problem_id, cp.name, cp.item,"
            " cp.method_of_choosing_final_submission, cp.score_revealing,"
            " MIN(s.submit_time) "
            "FROM ",
            record_set.sql_table_name,
            " cp LEFT JOIN ",
            record_set.sql_table_prefix,
            "submissions s "
            "ON s.contest_problem_id=cp.id GROUP BY cp.id"
        );
        stmt.bind_and_execute();
        stmt.res_bind_all(
            cp.id,
            cp.contest_round_id,
            cp.contest_id,
            cp.problem_id,
            cp.name,
            cp.item,
            cp.method_of_choosing_final_submission,
            cp.score_revealing,
            earliest_submit_time
        );

        auto curr_time = std::chrono::system_clock::now();
        while (stmt.next()) {
            cp.contest_round_id = contest_rounds_.new_id(cp.contest_round_id, record_set.kind);
            cp.contest_id = contests_.new_id(cp.contest_id, record_set.kind);
            cp.problem_id = problems_.new_id(cp.problem_id, record_set.kind);

            auto time =
                (earliest_submit_time.has_value()
                     ? str_to_time_point(
                           intentional_unsafe_cstring_view(earliest_submit_time->to_string())
                       )
                     : curr_time);
            record_set.add_record(cp, time);
        }
    }

    void merge() override {
        STACK_UNWINDING_MARK;
        Merger::merge([&](const sim::contest_problems::ContestProblem& /*unused*/) {
            return nullptr;
        });
    }

public:
    void save_merged() override {
        STACK_UNWINDING_MARK;
        auto transaction = conn.start_transaction();
        conn.update("TRUNCATE ", sql_table_name());
        auto stmt = conn.prepare(
            "INSERT INTO ",
            sql_table_name(),
            "(id, contest_round_id, contest_id,"
            " problem_id, name, item,"
            " method_of_choosing_final_submission, score_revealing) "
            "VALUES(?, ?, ?, ?, ?, ?, ?, ?)"
        );

        ProgressBar progress_bar("Contest problems saved:", new_table_.size(), 128);
        for (const NewRecord& new_record : new_table_) {
            Defer progressor = [&] { progress_bar.iter(); };
            const auto& x = new_record.data;
            stmt.bind_and_execute(
                x.id,
                x.contest_round_id,
                x.contest_id,
                x.problem_id,
                x.name,
                x.item,
                x.method_of_choosing_final_submission,
                x.score_revealing
            );
        }

        conn.update("ALTER TABLE ", sql_table_name(), " AUTO_INCREMENT=", last_new_id_ + 1);
        transaction.commit();
    }

    ContestProblemsMerger(
        const PrimaryKeysFromMainAndOtherJobs& ids_from_both_jobs,
        const ContestRoundsMerger& contest_rounds,
        const ContestsMerger& contests,
        const ProblemsMerger& problems
    )
    : Merger(
          "contest_problems",
          ids_from_both_jobs.main.contest_problems,
          ids_from_both_jobs.other.contest_problems
      )
    , contest_rounds_(contest_rounds)
    , contests_(contests)
    , problems_(problems) {
        STACK_UNWINDING_MARK;
        initialize();
    }
};

} // namespace sim_merger

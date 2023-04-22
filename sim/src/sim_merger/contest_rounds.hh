#pragma once

#include "contests.hh"

#include <sim/contest_rounds/contest_round.hh>
#include <sim/sql_fields/datetime.hh>

namespace sim_merger {

class ContestRoundsMerger : public Merger<sim::contest_rounds::ContestRound> {
    const ContestsMerger& contests_;

    void load(RecordSet& record_set) override {
        STACK_UNWINDING_MARK;

        sim::contest_rounds::ContestRound cr;
        mysql::Optional<sim::sql_fields::Datetime> earliest_submit_time;
        auto stmt = conn.prepare(
            "SELECT cr.id, cr.contest_id, cr.name, cr.item,"
            " cr.begins, cr.ends, cr.full_results,"
            " cr.ranking_exposure, MIN(s.submit_time) "
            "FROM ",
            record_set.sql_table_name,
            " cr LEFT JOIN ",
            record_set.sql_table_prefix,
            "submissions s "
            "ON s.contest_round_id=cr.id GROUP BY cr.id"
        );
        stmt.bind_and_execute();
        stmt.res_bind_all(
            cr.id,
            cr.contest_id,
            cr.name,
            cr.item,
            cr.begins,
            cr.ends,
            cr.full_results,
            cr.ranking_exposure,
            earliest_submit_time
        );
        auto curr_time = std::chrono::system_clock::now();
        while (stmt.next()) {
            cr.contest_id = contests_.new_id(cr.contest_id, record_set.kind);

            auto time =
                (earliest_submit_time.has_value()
                     ? str_to_time_point(from_unsafe{earliest_submit_time->to_string()})
                     : curr_time);
            record_set.add_record(cr, time);
        }
    }

    void merge() override {
        STACK_UNWINDING_MARK;
        Merger::merge([&](const sim::contest_rounds::ContestRound& /*unused*/) { return nullptr; });
    }

public:
    void save_merged() override {
        STACK_UNWINDING_MARK;
        auto transaction = conn.start_transaction();
        conn.update("TRUNCATE ", sql_table_name());
        auto stmt = conn.prepare(
            "INSERT INTO ",
            sql_table_name(),
            "(id, contest_id, name, item, begins, ends,"
            " full_results, ranking_exposure) "
            "VALUES(?, ?, ?, ?, ?, ?, ?, ?)"
        );

        ProgressBar progress_bar("Contest rounds saved:", new_table_.size(), 128);
        for (const NewRecord& new_record : new_table_) {
            Defer progressor = [&] { progress_bar.iter(); };
            const auto& x = new_record.data;
            stmt.bind_and_execute(
                x.id,
                x.contest_id,
                x.name,
                x.item,
                x.begins,
                x.ends,
                x.full_results,
                x.ranking_exposure
            );
        }

        conn.update("ALTER TABLE ", sql_table_name(), " AUTO_INCREMENT=", last_new_id_ + 1);
        transaction.commit();
    }

    ContestRoundsMerger(
        const PrimaryKeysFromMainAndOtherJobs& ids_from_both_jobs, const ContestsMerger& contests
    )
    : Merger(
          "contest_rounds",
          ids_from_both_jobs.main.contest_rounds,
          ids_from_both_jobs.other.contest_rounds
      )
    , contests_(contests) {
        STACK_UNWINDING_MARK;
        initialize();
    }
};

} // namespace sim_merger

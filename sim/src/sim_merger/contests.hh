#pragma once

#include "merger.hh"

#include <sim/contests/contest.hh>
#include <sim/sql_fields/datetime.hh>
#include <simlib/defer.hh>

namespace sim_merger {

class ContestsMerger : public Merger<sim::contests::Contest> {
    void load(RecordSet& record_set) override {
        STACK_UNWINDING_MARK;

        sim::contests::Contest c;
        mysql::Optional<sim::sql_fields::Datetime> earliest_submit_time;
        auto stmt = conn.prepare(
            "SELECT c.id, c.name, c.is_public, MIN(s.submit_time) "
            "FROM ",
            record_set.sql_table_name,
            " c LEFT JOIN ",
            record_set.sql_table_prefix,
            "submissions s "
            "ON s.contest_id=c.id GROUP BY c.id"
        );
        stmt.bind_and_execute();
        stmt.res_bind_all(c.id, c.name, c.is_public, earliest_submit_time);

        auto curr_time = std::chrono::system_clock::now();
        while (stmt.next()) {
            auto time =
                (earliest_submit_time.has_value()
                     ? str_to_time_point(from_unsafe{earliest_submit_time->to_string()})
                     : curr_time);
            record_set.add_record(c, time);
        }
    }

    void merge() override {
        STACK_UNWINDING_MARK;
        Merger::merge([&](const sim::contests::Contest& /*unused*/) { return nullptr; });
    }

public:
    void save_merged() override {
        STACK_UNWINDING_MARK;
        auto transaction = conn.start_transaction();
        conn.update("TRUNCATE ", sql_table_name());
        auto stmt =
            conn.prepare("INSERT INTO ", sql_table_name(), "(id, name, is_public) VALUES(?, ?, ?)");

        ProgressBar progress_bar("Contests saved:", new_table_.size(), 128);
        for (const NewRecord& new_record : new_table_) {
            Defer progressor = [&] { progress_bar.iter(); };
            const auto& x = new_record.data;
            stmt.bind_and_execute(x.id, x.name, x.is_public);
        }

        conn.update("ALTER TABLE ", sql_table_name(), " AUTO_INCREMENT=", last_new_id_ + 1);
        transaction.commit();
    }

    explicit ContestsMerger(const PrimaryKeysFromMainAndOtherJobs& ids_from_both_jobs)
    : Merger("contests", ids_from_both_jobs.main.contests, ids_from_both_jobs.other.contests) {
        STACK_UNWINDING_MARK;
        initialize();
    }
};

} // namespace sim_merger

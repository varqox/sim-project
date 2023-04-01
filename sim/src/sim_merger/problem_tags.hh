#pragma once

#include "merger.hh"
#include "primary_keys_from_jobs.hh"
#include "problems.hh"

#include <sim/problem_tags/problem_tag.hh>

namespace sim_merger {

class ProblemTagsMerger : public Merger<sim::problem_tags::ProblemTag> {
    const ProblemsMerger& problems_;

    void load(RecordSet& record_set) override {
        STACK_UNWINDING_MARK;

        sim::problem_tags::ProblemTag ptag;
        auto stmt =
            conn.prepare("SELECT problem_id, name, is_hidden FROM ", record_set.sql_table_name);
        stmt.bind_and_execute();
        stmt.res_bind_all(ptag.problem_id, ptag.name, ptag.is_hidden);
        while (stmt.next()) {
            ptag.problem_id = problems_.new_id(ptag.problem_id, record_set.kind);
            // Time does not matter
            record_set.add_record(ptag, std::chrono::system_clock::time_point::min());
        }
    }

    void merge() override {
        STACK_UNWINDING_MARK;
        Merger::merge([&](const sim::problem_tags::ProblemTag& /*unused*/) { return nullptr; });
    }

    PrimaryKeyType pre_merge_record_id_to_post_merge_record_id(const PrimaryKeyType& record_id
    ) override {
        return record_id;
    }

public:
    void save_merged() override {
        STACK_UNWINDING_MARK;
        auto transaction = conn.start_transaction();
        conn.update("TRUNCATE ", sql_table_name());
        auto stmt = conn.prepare(
            "INSERT INTO ",
            sql_table_name(),
            "(problem_id, name, is_hidden) "
            "VALUES(?, ?, ?)"
        );

        ProgressBar progress_bar("Problem tags saved:", new_table_.size(), 128);
        for (const NewRecord& new_record : new_table_) {
            Defer progressor = [&] { progress_bar.iter(); };
            const auto& x = new_record.data;
            stmt.bind_and_execute(x.problem_id, x.name, x.is_hidden);
        }

        transaction.commit();
    }

    ProblemTagsMerger(
        const PrimaryKeysFromMainAndOtherJobs& ids_from_both_jobs, const ProblemsMerger& problems
    )
    : Merger(
          "problem_tags",
          ids_from_both_jobs.main.problem_tags,
          ids_from_both_jobs.other.problem_tags
      )
    , problems_(problems) {
        STACK_UNWINDING_MARK;
        initialize();
    }
};

} // namespace sim_merger

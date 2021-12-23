#pragma once

#include "sim/problem_tags/problem_tag.hh"
#include "src/sim_merger/ids_from_jobs.hh"
#include "src/sim_merger/merger.hh"
#include "src/sim_merger/problems.hh"

namespace sim_merger {

class ProblemTagsMerger
: public Merger<sim::problem_tags::ProblemTag, DefaultIdGetter, ProblemTagIdCmp> {
    const ProblemsMerger& problems_;

    void load(RecordSet& record_set) override {
        STACK_UNWINDING_MARK;

        sim::problem_tags::ProblemTag ptag;
        auto stmt = conn.prepare(
                "SELECT problem_id, tag, hidden FROM ", record_set.sql_table_name);
        stmt.bind_and_execute();
        stmt.res_bind_all(ptag.id.problem_id, ptag.id.tag, ptag.hidden);
        while (stmt.next()) {
            ptag.id.problem_id = problems_.new_id(ptag.id.problem_id, record_set.kind);
            // Time does not matter
            record_set.add_record(ptag, std::chrono::system_clock::time_point::min());
        }
    }

    void merge() override {
        STACK_UNWINDING_MARK;
        Merger::merge(
                [&](const sim::problem_tags::ProblemTag& /*unused*/) { return nullptr; });
    }

    decltype(sim::problem_tags::ProblemTag::id) pre_merge_record_id_to_post_merge_record_id(
            const decltype(sim::problem_tags::ProblemTag::id)& record_id) override {
        return record_id;
    }

public:
    void save_merged() override {
        STACK_UNWINDING_MARK;
        auto transaction = conn.start_transaction();
        conn.update("TRUNCATE ", sql_table_name());
        auto stmt = conn.prepare("INSERT INTO ", sql_table_name(),
                "(problem_id, tag, hidden) "
                "VALUES(?, ?, ?)");

        ProgressBar progress_bar("Problem tags saved:", new_table_.size(), 128);
        for (const NewRecord& new_record : new_table_) {
            Defer progressor = [&] { progress_bar.iter(); };
            const auto& x = new_record.data;
            stmt.bind_and_execute(x.id.problem_id, x.id.tag, x.hidden);
        }

        transaction.commit();
    }
    ProblemTagsMerger(
            const IdsFromMainAndOtherJobs& ids_from_both_jobs, const ProblemsMerger& problems)
    : Merger("problem_tags", ids_from_both_jobs.main.problem_tags,
              ids_from_both_jobs.other.problem_tags)
    , problems_(problems) {
        STACK_UNWINDING_MARK;
        initialize();
    }
};

} // namespace sim_merger

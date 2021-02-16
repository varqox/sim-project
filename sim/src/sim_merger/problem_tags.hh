#pragma once

#include "src/sim_merger/problems.hh"

struct ProblemTag {
    ProblemTagId id;
    bool hidden = false;
};

class ProblemTagsMerger : public Merger<ProblemTag> {
    const ProblemsMerger& problems_;

    void load(RecordSet& record_set) override {
        STACK_UNWINDING_MARK;

        ProblemTag ptag;
        uint8_t b_hidden = false;
        auto stmt =
            conn.prepare("SELECT problem_id, tag, hidden FROM ", record_set.sql_table_name);
        stmt.bind_and_execute();
        stmt.res_bind_all(ptag.id.problem_id, ptag.id.tag, b_hidden);
        while (stmt.next()) {
            ptag.hidden = b_hidden;

            ptag.id.problem_id = problems_.new_id(ptag.id.problem_id, record_set.kind);
            // Time does not matter
            record_set.add_record(ptag, std::chrono::system_clock::time_point::min());
        }
    }

    void merge() override {
        STACK_UNWINDING_MARK;
        Merger::merge([&](const ProblemTag& /*unused*/) { return nullptr; });
    }

    ProblemTagId
    new_id_for_record_to_merge_into_new_records(const ProblemTagId& record_id) override {
        return record_id;
    }

public:
    void save_merged() override {
        STACK_UNWINDING_MARK;
        auto transaction = conn.start_transaction();
        conn.update("TRUNCATE ", sql_table_name());
        auto stmt = conn.prepare(
            "INSERT INTO ", sql_table_name(),
            "(problem_id, tag, hidden) "
            "VALUES(?, ?, ?)");

        ProgressBar progress_bar("Problem tags saved:", new_table_.size(), 128);
        for (const NewRecord& new_record : new_table_) {
            Defer progressor = [&] { progress_bar.iter(); };
            const ProblemTag& x = new_record.data;
            stmt.bind_and_execute(x.id.problem_id, x.id.tag, x.hidden);
        }

        transaction.commit();
    }
    ProblemTagsMerger(
        const IdsFromMainAndOtherJobs& ids_from_both_jobs, const ProblemsMerger& problems)
    : Merger(
          "problem_tags", ids_from_both_jobs.main.problem_tags,
          ids_from_both_jobs.other.problem_tags)
    , problems_(problems) {
        STACK_UNWINDING_MARK;
        initialize();
    }
};

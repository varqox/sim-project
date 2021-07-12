#pragma once

#include "sim/contest_files/contest_file.hh"
#include "sim/random.hh"
#include "src/sim_merger/contests.hh"
#include "src/sim_merger/internal_files.hh"
#include "src/sim_merger/users.hh"

#include <set>

namespace sim_merger {

class ContestFilesMerger : public Merger<sim::contest_files::ContestFile> {
    const InternalFilesMerger& internal_files_;
    const ContestsMerger& contests_;
    const UsersMerger& users_;

    std::set<decltype(sim::contest_files::ContestFile::id)> taken_contest_files_ids_;

    void load(RecordSet& record_set) override {
        STACK_UNWINDING_MARK;

        sim::contest_files::ContestFile cf;
        mysql::Optional<decltype(cf.creator)::value_type> m_creator;
        auto stmt = conn.prepare(
            "SELECT id, file_id, contest_id, name, description, "
            "file_size, modified, creator FROM ",
            record_set.sql_table_name);
        stmt.bind_and_execute();
        stmt.res_bind_all(
            cf.id, cf.file_id, cf.contest_id, cf.name, cf.description, cf.file_size,
            cf.modified, m_creator);
        while (stmt.next()) {
            cf.creator = m_creator.to_opt();

            cf.file_id = internal_files_.new_id(cf.file_id, record_set.kind);
            cf.contest_id = contests_.new_id(cf.contest_id, record_set.kind);
            if (cf.creator) {
                cf.creator = users_.new_id(cf.creator.value(), record_set.kind);
            }

            // Time does not matter
            record_set.add_record(cf, std::chrono::system_clock::time_point::min());
        }
    }

    void merge() override {
        STACK_UNWINDING_MARK;
        Merger::merge(
            [&](const sim::contest_files::ContestFile& /*unused*/) { return nullptr; });
    }

    decltype(sim::contest_files::ContestFile::id) pre_merge_record_id_to_post_merge_record_id(
        const decltype(sim::contest_files::ContestFile::id)& record_id) override {
        STACK_UNWINDING_MARK;
        std::string new_id = record_id.to_string();
        while (not taken_contest_files_ids_.emplace(new_id).second) {
            new_id = sim::generate_random_token(
                decltype(sim::contest_files::ContestFile::id)::max_len);
        }
        return decltype(sim::contest_files::ContestFile::id)(new_id);
    }

public:
    void save_merged() override {
        STACK_UNWINDING_MARK;
        auto transaction = conn.start_transaction();
        conn.update("TRUNCATE ", sql_table_name());
        auto stmt = conn.prepare(
            "INSERT INTO ", sql_table_name(),
            "(id, file_id, contest_id, name, description,"
            " file_size, modified, creator) "
            "VALUES(?, ?, ?, ?, ?, ?, ?, ?)");

        ProgressBar progress_bar("Contest files saved:", new_table_.size(), 128);
        for (const NewRecord& new_record : new_table_) {
            Defer progressor = [&] { progress_bar.iter(); };
            const auto& x = new_record.data;
            stmt.bind_and_execute(
                x.id, x.file_id, x.contest_id, x.name, x.description, x.file_size, x.modified,
                x.creator);
        }

        transaction.commit();
    }

    ContestFilesMerger(
        const IdsFromMainAndOtherJobs& ids_from_both_jobs,
        const InternalFilesMerger& internal_files, const ContestsMerger& contests,
        const UsersMerger& users)
    : Merger(
          "contest_files", ids_from_both_jobs.main.contest_files,
          ids_from_both_jobs.other.contest_files)
    , internal_files_(internal_files)
    , contests_(contests)
    , users_(users) {
        STACK_UNWINDING_MARK;
        initialize();
    }
};

} // namespace sim_merger

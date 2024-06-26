#pragma once

#include "contests.hh"
#include "internal_files.hh"
#include "users.hh"

#include <set>
#include <sim/contest_files/old_contest_file.hh>
#include <sim/random.hh>

namespace sim_merger {

class ContestFilesMerger : public Merger<sim::contest_files::OldContestFile> {
    const InternalFilesMerger& internal_files_;
    const ContestsMerger& contests_;
    const UsersMerger& users_;

    std::set<decltype(sim::contest_files::OldContestFile::id)> taken_contest_files_ids_;

    void load(RecordSet& record_set) override {
        STACK_UNWINDING_MARK;

        sim::contest_files::OldContestFile cf;
        old_mysql::Optional<decltype(cf.creator)::value_type> m_creator;
        auto old_mysql = old_mysql::ConnectionView{*mysql};
        auto stmt = old_mysql.prepare(
            "SELECT id, file_id, contest_id, name, description, "
            "file_size, modified, creator FROM ",
            record_set.sql_table_name
        );
        stmt.bind_and_execute();
        stmt.res_bind_all(
            cf.id,
            cf.file_id,
            cf.contest_id,
            cf.name,
            cf.description,
            cf.file_size,
            cf.modified,
            m_creator
        );
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
        Merger::merge([&](const sim::contest_files::OldContestFile& /*unused*/) { return nullptr; }
        );
    }

    PrimaryKeyType pre_merge_record_id_to_post_merge_record_id(const PrimaryKeyType& record_id
    ) override {
        STACK_UNWINDING_MARK;
        std::string new_id = record_id.to_string();
        while (not taken_contest_files_ids_.emplace(new_id).second) {
            new_id = sim::generate_random_token(decltype(sim::contest_files::OldContestFile::id
            )::max_len);
        }
        return decltype(sim::contest_files::OldContestFile::id)(new_id);
    }

public:
    void save_merged() override {
        STACK_UNWINDING_MARK;
        auto transaction = mysql->start_repeatable_read_transaction();
        auto old_mysql = old_mysql::ConnectionView{*mysql};
        old_mysql.update("TRUNCATE ", sql_table_name());
        auto stmt = old_mysql.prepare(
            "INSERT INTO ",
            sql_table_name(),
            "(id, file_id, contest_id, name, description,"
            " file_size, modified, creator) "
            "VALUES(?, ?, ?, ?, ?, ?, ?, ?)"
        );

        ProgressBar progress_bar("Contest files saved:", new_table_.size(), 128);
        for (const NewRecord& new_record : new_table_) {
            Defer progressor = [&] { progress_bar.iter(); };
            const auto& x = new_record.data;
            stmt.bind_and_execute(
                x.id,
                x.file_id,
                x.contest_id,
                x.name,
                x.description,
                x.file_size,
                x.modified,
                x.creator
            );
        }

        transaction.commit();
    }

    ContestFilesMerger(
        const PrimaryKeysFromMainAndOtherJobs& ids_from_both_jobs,
        const InternalFilesMerger& internal_files,
        const ContestsMerger& contests,
        const UsersMerger& users
    )
    : Merger(
          "contest_files",
          ids_from_both_jobs.main.contest_files,
          ids_from_both_jobs.other.contest_files
      )
    , internal_files_(internal_files)
    , contests_(contests)
    , users_(users) {
        STACK_UNWINDING_MARK;
        initialize();
    }
};

} // namespace sim_merger

#pragma once

#include "sim/random.hh"
#include "src/sim_merger/contests.hh"
#include "src/sim_merger/internal_files.hh"
#include "src/sim_merger/users.hh"

#include <set>

struct ContestFile {
    InplaceBuff<FILE_ID_LEN> id;
    uintmax_t file_id{};
    uintmax_t contest_id{};
    InplaceBuff<FILE_NAME_MAX_LEN> name;
    InplaceBuff<FILE_DESCRIPTION_MAX_LEN> description;
    uintmax_t file_size{};
    InplaceBuff<24> modified;
    std::optional<uintmax_t> creator;
};

class ContestFilesMerger : public Merger<ContestFile> {
    const InternalFilesMerger& internal_files_;
    const ContestsMerger& contests_;
    const UsersMerger& users_;

    std::set<InplaceBuff<FILE_ID_LEN>> taken_contest_files_ids_;

    void load(RecordSet& record_set) override {
        STACK_UNWINDING_MARK;

        ContestFile cf;
        MySQL::Optional<uintmax_t> m_creator;
        auto stmt = conn.prepare(
            "SELECT id, file_id, contest_id, name, description, "
            "file_size, modified, creator FROM ",
            record_set.sql_table_name);
        stmt.bind_and_execute();
        stmt.res_bind_all(
            cf.id, cf.file_id, cf.contest_id, cf.name, cf.description, cf.file_size,
            cf.modified, m_creator);
        while (stmt.next()) {
            cf.creator = m_creator.opt();

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
        Merger::merge([&](const ContestFile& /*unused*/) { return nullptr; });
    }

    InplaceBuff<FILE_ID_LEN> new_id_for_record_to_merge_into_new_records(
        const InplaceBuff<FILE_ID_LEN>& record_id) override {
        STACK_UNWINDING_MARK;
        std::string new_id = record_id.to_string();
        while (not taken_contest_files_ids_.emplace(new_id).second) {
            new_id = generate_random_token(FILE_ID_LEN);
        }
        return InplaceBuff<FILE_ID_LEN>(new_id);
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
            const ContestFile& x = new_record.data;
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

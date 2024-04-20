#pragma once

#include "merger.hh"

#include <sim/internal_files/old_internal_file.hh>
#include <simlib/defer.hh>
#include <simlib/file_info.hh>
#include <simlib/file_manip.hh>
#include <simlib/time.hh>

namespace sim_merger {

class InternalFilesMerger : public Merger<sim::internal_files::OldInternalFile> {
    void load(RecordSet& record_set) override {
        STACK_UNWINDING_MARK;
        sim::internal_files::OldInternalFile file{};
        auto old_mysql = old_mysql::ConnectionView{*mysql};
        auto stmt = old_mysql.prepare("SELECT id FROM ", record_set.sql_table_name);
        stmt.bind_and_execute();
        stmt.res_bind_all(file.id);
        while (stmt.next()) {
            auto mtime =
                get_modification_time(concat(record_set.sim_build(), "internal_files/", file.id));
            record_set.add_record(file, mtime);
        }
    }

    void merge() override {
        STACK_UNWINDING_MARK;
        Merger::merge([&](const sim::internal_files::OldInternalFile& /*unused*/) -> NewRecord* {
            return nullptr;
        });
    }

    static auto internal_files_backup_path() {
        return concat<PATH_MAX>(main_sim_build, "internal_files.before_merge/");
    }

    static auto internal_files_trash_path() {
        return concat<PATH_MAX>(main_sim_build, "internal_files.merge_trash/");
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
            "(id) "
            "VALUES(?)"
        );

        THROW("TODO: created_at");

        auto bkp_path = internal_files_backup_path();
        auto dest_path = concat(main_sim_build, "internal_files/");
        auto trash_path = internal_files_trash_path();
        if (remove_r(trash_path) and errno != ENOENT) {
            THROW("remove_r()", errmsg());
        }
        if (remove_r(bkp_path) and errno != ENOENT) {
            THROW("remove_r()", errmsg());
        }

        if (rename(dest_path, bkp_path)) {
            THROW("rename()", errmsg());
        }

        bool saving_successful = false;
        Defer internal_files_restorer([&] {
            if (saving_successful) {
                return;
            }

            if (rename(dest_path, trash_path)) {
                (void)remove_r(dest_path);
            }

            (void)rename(bkp_path, dest_path);
            (void)remove_r(trash_path);
        });

        if (mkdir(dest_path)) {
            THROW("mkdir()", errmsg());
        }

        decltype(sim::internal_files::OldInternalFile::id) id = 0;
        stmt.bind_all(id);

        ProgressBar progress_bar("Internal files saved:", new_table_.size(), 128);
        for (const NewRecord& new_record : new_table_) {
            Defer progressor = [&] { progress_bar.iter(); };
            const auto& x = new_record.data;
            id = x.id;
            stmt.execute();

            if (not new_record.main_ids.empty()) {
                // Hard link main's files
                auto src = concat(bkp_path, new_record.main_ids.front());
                auto dest = concat(dest_path, x.id);
                if (link(src, dest)) {
                    THROW("link(", src, ", ", dest, ')', errmsg());
                }
            } else {
                // Copy other's files
                throw_assert(not new_record.other_ids.empty());
                auto src = concat(other_sim_build, "internal_files/", new_record.other_ids.front());
                auto dest = concat(dest_path, x.id);
                if (copy(src, dest)) {
                    THROW("copy(", src, ", ", dest, ')', errmsg());
                }
            }
        }

        old_mysql.update("ALTER TABLE ", sql_table_name(), " AUTO_INCREMENT=", last_new_id_ + 1);
        transaction.commit();
        saving_successful = true;
    }

    void rollback_saving_merged_outside_database() noexcept override {
        auto bkp_path = internal_files_backup_path();
        auto dest_path = concat<PATH_MAX>(main_sim_build, "internal_files/");
        auto trash_path = internal_files_trash_path();

        if (rename(dest_path, trash_path)) {
            (void)remove_r(dest_path);
        }

        (void)rename(bkp_path, dest_path);
        (void)remove_r(trash_path);
    }

    explicit InternalFilesMerger(const PrimaryKeysFromMainAndOtherJobs& ids_from_both_jobs)
    : Merger(
          "internal_files",
          ids_from_both_jobs.main.internal_files,
          ids_from_both_jobs.other.internal_files
      ) {
        STACK_UNWINDING_MARK;
        initialize();
    }

    auto path_to_file(decltype(sim::internal_files::OldInternalFile::id) new_id) const {
        STACK_UNWINDING_MARK;
        if (new_table_.empty()) {
            THROW("Invalid new_id");
        }

        size_t b = 0;
        size_t e = new_table_.size() - 1;
        while (b < e) {
            auto m = b + (e - b) / 2;
            if (new_table_[m].data.id < new_id) {
                b = m + 1;
            } else {
                e = m;
            }
        }

        auto& elem = new_table_[b];
        if (elem.data.id != new_id) {
            THROW("Invalid new_id");
        }

        if (not elem.main_ids.empty()) {
            return concat(main_sim_build, "internal_files/", elem.main_ids.front());
        }
        if (not elem.other_ids.empty()) {
            return concat(other_sim_build, "internal_files/", elem.other_ids.front());
        }

        THROW("Invalid new_id");
    }
};

} // namespace sim_merger

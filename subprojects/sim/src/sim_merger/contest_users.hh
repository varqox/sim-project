#pragma once

#include "contests.hh"
#include "merger.hh"
#include "users.hh"

#include <sim/contest_users/old_contest_user.hh>

namespace sim_merger {

class ContestUsersMerger : public Merger<sim::contest_users::OldContestUser> {
    const UsersMerger& users_;
    const ContestsMerger& contests_;

    void load(RecordSet& record_set) override {
        STACK_UNWINDING_MARK;

        sim::contest_users::OldContestUser cu{};
        auto old_mysql = old_mysql::ConnectionView{*mysql};
        auto stmt =
            old_mysql.prepare("SELECT user_id, contest_id, mode FROM ", record_set.sql_table_name);
        stmt.bind_and_execute();
        stmt.res_bind_all(cu.user_id, cu.contest_id, cu.mode);
        while (stmt.next()) {
            cu.user_id = users_.new_id(cu.user_id, record_set.kind);
            cu.contest_id = contests_.new_id(cu.contest_id, record_set.kind);
            // Time does not matter
            record_set.add_record(cu, std::chrono::system_clock::time_point::min());
        }
    }

    void merge() override {
        STACK_UNWINDING_MARK;
        Merger::merge([&](const sim::contest_users::OldContestUser& /*unused*/) { return nullptr; }
        );
    }

    PrimaryKeyType pre_merge_record_id_to_post_merge_record_id(const PrimaryKeyType& record_id
    ) override {
        return record_id;
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
            "(user_id, contest_id, mode) "
            "VALUES(?, ?, ?)"
        );

        ProgressBar progress_bar("Contest users saved:", new_table_.size(), 128);
        for (const NewRecord& new_record : new_table_) {
            Defer progressor = [&] { progress_bar.iter(); };
            const auto& x = new_record.data;
            stmt.bind_and_execute(x.user_id, x.contest_id, x.mode);
        }

        transaction.commit();
    }

    ContestUsersMerger(
        const PrimaryKeysFromMainAndOtherJobs& ids_from_both_jobs,
        const UsersMerger& users,
        const ContestsMerger& contests
    )
    : Merger(
          "contest_users",
          ids_from_both_jobs.main.contest_users,
          ids_from_both_jobs.other.contest_users
      )
    , users_(users)
    , contests_(contests) {
        STACK_UNWINDING_MARK;
        initialize();
    }
};

} // namespace sim_merger

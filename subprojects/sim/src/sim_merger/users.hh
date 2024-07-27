#pragma once

#include "merger.hh"

#include <sim/submissions/old_submission.hh>
#include <sim/submissions/update_final.hh>
#include <sim/users/old_user.hh>
#include <simlib/defer.hh>

namespace sim_merger {

// Merges users by username
class UsersMerger : public Merger<sim::users::OldUser> {
    std::map<decltype(sim::users::OldUser::username), size_t> username_to_new_table_idx_;

    void load(RecordSet& record_set) override {
        STACK_UNWINDING_MARK;
        // Prefer main users to come first
        auto time = [&] {
            switch (record_set.kind) {
            case IdKind::Main: return std::chrono::system_clock::from_time_t(0);
            case IdKind::Other: return std::chrono::system_clock::time_point::max();
            }

            THROW("BUG");
        }();

        THROW("TODO: use created_at");

        sim::users::OldUser user;
        auto old_mysql = old_mysql::ConnectionView{*mysql};
        auto stmt = old_mysql.prepare(
            "SELECT id, type, username, first_name, last_name, "
            "email, password_salt, password_hash FROM ",
            record_set.sql_table_name
        );
        stmt.bind_and_execute();
        stmt.res_bind_all(
            user.id,
            user.type,
            user.username,
            user.first_name,
            user.last_name,
            user.email,
            user.password_salt,
            user.password_hash
        );

        while (stmt.next()) {
            record_set.add_record(user, time);
        }
    }

    void merge() override {
        STACK_UNWINDING_MARK;
        Merger::merge([&](const sim::users::OldUser& x, IdKind kind) -> NewRecord* {
            auto it = username_to_new_table_idx_.find(x.username);
            if (it != username_to_new_table_idx_.end()) {
                NewRecord& res = new_table_[it->second];
                stdlog(
                    "\nMerging: ",
                    x.username,
                    '\t',
                    x.first_name,
                    '\t',
                    x.last_name,
                    '\t',
                    x.email,
                    ' ',
                    id_info(x, kind),
                    "\n   into: ",
                    res.data.username,
                    '\t',
                    res.data.first_name,
                    '\t',
                    res.data.last_name,
                    '\t',
                    res.data.email,
                    ' ',
                    id_info(res)
                );
                return &res;
            }

            username_to_new_table_idx_[x.username] = new_table_.size();
            return nullptr;
        });
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
            "(id, type, username, first_name, last_name, email,"
            " password_salt, password_hash) "
            "VALUES(?, ?, ?, ?, ?, ?, ?, ?)"
        );

        ProgressBar progress_bar("Users saved:", new_table_.size(), 128);
        for (const NewRecord& new_record : new_table_) {
            Defer progressor = [&] { progress_bar.iter(); };
            const auto& x = new_record.data;
            stmt.bind_and_execute(
                x.id,
                x.type,
                x.username,
                x.first_name,
                x.last_name,
                x.email,
                x.password_salt,
                x.password_hash
            );
        }

        old_mysql.update("ALTER TABLE ", sql_table_name(), " AUTO_INCREMENT=", last_new_id_ + 1);
        transaction.commit();
    }

    explicit UsersMerger(const PrimaryKeysFromMainAndOtherJobs& ids_from_both_jobs)
    : Merger("users", ids_from_both_jobs.main.users, ids_from_both_jobs.other.users) {
        STACK_UNWINDING_MARK;
        initialize();
    }

    void run_after_saving_hooks() override {
        // Reselect final submissions for merged users originating from single
        // source
        auto transaction = mysql->start_repeatable_read_transaction();
        for (const auto& user : new_table_) {
            if (user.main_ids.size() > 1 or user.other_ids.size() > 1) {
                decltype(sim::submissions::OldSubmission::problem_id) problem_id = 0;
                old_mysql::Optional<decltype(sim::submissions::OldSubmission::contest_problem_id
                )::value_type>
                    contest_problem_id;

                auto old_mysql = old_mysql::ConnectionView{*mysql};
                auto stmt = old_mysql.prepare("SELECT problem_id, contest_problem_id "
                                              "FROM submissions "
                                              "WHERE user_id=? "
                                              "GROUP BY problem_id, contest_problem_id");
                stmt.bind_and_execute(user.data.id);
                stmt.res_bind_all(problem_id, contest_problem_id);
                while (stmt.next()) {
                    sim::submissions::update_final(
                        *mysql, user.data.id, problem_id, contest_problem_id
                    );
                }
            }
        }
        // Final submissions for problems do not have to be reselect, because
        // problem are not merged during sim-merge -- they are merged using
        // MERGE_PROBLEMS jobs and there final submissions are reselected.

        transaction.commit();
    }
};

} // namespace sim_merger

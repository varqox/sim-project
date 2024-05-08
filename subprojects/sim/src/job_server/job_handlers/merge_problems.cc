#include "merge_problems.hh"

#include <deque>
#include <sim/submissions/old_submission.hh>
#include <sim/submissions/update_final.hh>

using sim::jobs::OldJob;
using sim::submissions::OldSubmission;

namespace job_server::job_handlers {

void MergeProblems::run(sim::mysql::Connection& mysql) {
    STACK_UNWINDING_MARK;

    for (;;) {
        try {
            run_impl(mysql);
            break;
        } catch (const std::exception& e) {
            if (has_prefix(
                    e.what(),
                    "Deadlock found when trying to get lock; "
                    "try restarting transaction"
                ))
            {
                continue;
            }

            throw;
        }
    }
}

void MergeProblems::run_impl(sim::mysql::Connection& mysql) {
    STACK_UNWINDING_MARK;

    auto transaction = mysql.start_repeatable_read_transaction();
    auto old_mysql = old_mysql::ConnectionView{mysql};

    // Assure that both problems exist
    {
        auto stmt = old_mysql.prepare("SELECT simfile FROM problems WHERE id=?");
        stmt.bind_and_execute(donor_problem_id_);
        InplaceBuff<0> simfile;
        stmt.res_bind_all(simfile);
        if (not stmt.next()) {
            return set_failure("Problem does not exist");
        }

        job_log("Merged problem Simfile:\n", simfile);

        stmt = old_mysql.prepare("SELECT 1 FROM problems WHERE id=?");
        stmt.bind_and_execute(info_.target_problem_id);
        int x;
        stmt.res_bind_all(x);
        if (not stmt.next()) {
            return set_failure("Target problem does not exist");
        }
    }

    // Transfer contest problems
    old_mysql.prepare("UPDATE contest_problems SET problem_id=? WHERE problem_id=?")
        .bind_and_execute(info_.target_problem_id, donor_problem_id_);

    // Add job to delete problem file
    old_mysql
        .prepare("INSERT INTO jobs(file_id, creator, type, priority, status,"
                 " created_at, aux_id, info, data) "
                 "SELECT file_id, NULL, ?, ?, ?, ?, NULL, '', '' "
                 "FROM problems WHERE id=?")
        .bind_and_execute(
            EnumVal(OldJob::Type::DELETE_FILE),
            default_priority(OldJob::Type::DELETE_FILE),
            EnumVal(OldJob::Status::PENDING),
            mysql_date(),
            donor_problem_id_
        );

    // Add jobs to delete problem solutions' files
    old_mysql
        .prepare("INSERT INTO jobs(file_id, creator, type, priority, status,"
                 " created_at, aux_id, info, data) "
                 "SELECT file_id, NULL, ?, ?, ?, ?, NULL, '', '' "
                 "FROM submissions WHERE problem_id=? AND "
                 "type=?")
        .bind_and_execute(
            EnumVal(OldJob::Type::DELETE_FILE),
            default_priority(OldJob::Type::DELETE_FILE),
            EnumVal(OldJob::Status::PENDING),
            mysql_date(),
            donor_problem_id_,
            EnumVal(OldSubmission::Type::PROBLEM_SOLUTION)
        );

    // Delete problem solutions
    old_mysql
        .prepare("DELETE FROM submissions "
                 "WHERE problem_id=? AND type=?")
        .bind_and_execute(donor_problem_id_, EnumVal(OldSubmission::Type::PROBLEM_SOLUTION));

    // Collect update finals
    struct FTU {
        old_mysql::Optional<uint64_t> owner;
        old_mysql::Optional<uint64_t> contest_problem_id;
    };

    std::deque<FTU> finals_to_update;
    {
        auto stmt = old_mysql.prepare("SELECT DISTINCT owner, contest_problem_id "
                                      "FROM submissions WHERE problem_id=?");
        stmt.bind_and_execute(donor_problem_id_);
        FTU ftu_elem;
        stmt.res_bind_all(ftu_elem.owner, ftu_elem.contest_problem_id);
        while (stmt.next()) {
            finals_to_update.emplace_back(ftu_elem);
        }
    }

    // Schedule rejudge of the transferred submissions
    if (info_.rejudge_transferred_submissions) {
        old_mysql
            .prepare("INSERT INTO jobs(creator, status, priority, type, created_at,"
                     " aux_id, info, data) "
                     "SELECT NULL, ?, ?, ?, ?, id, '', '' "
                     "FROM submissions WHERE problem_id=? ORDER BY id")
            .bind_and_execute(
                EnumVal(OldJob::Status::PENDING),
                default_priority(OldJob::Type::REJUDGE_SUBMISSION),
                EnumVal(OldJob::Type::REJUDGE_SUBMISSION),
                mysql_date(),
                donor_problem_id_
            );
    }

    // Transfer problem submissions that are not problem solutions
    old_mysql.prepare("UPDATE submissions SET problem_id=? WHERE problem_id=?")
        .bind_and_execute(info_.target_problem_id, donor_problem_id_);

    // Update finals (both contest and problem finals are being taken care of)
    for (const auto& ftu_elem : finals_to_update) {
        sim::submissions::update_final_lock(mysql, ftu_elem.owner, info_.target_problem_id);
        sim::submissions::update_final(
            mysql, ftu_elem.owner, info_.target_problem_id, ftu_elem.contest_problem_id, false
        );
    }

    // Transfer problem tags (duplicates will not be transferred - they will be
    // deleted on problem deletion)
    old_mysql.prepare("UPDATE IGNORE problem_tags SET problem_id=? WHERE problem_id=?")
        .bind_and_execute(info_.target_problem_id, donor_problem_id_);

    // Finally, delete the donor problem (solution and remaining tags will be
    // deleted automatically thanks to foreign key constraints)
    old_mysql.prepare("DELETE FROM problems WHERE id=?").bind_and_execute(donor_problem_id_);

    job_done(mysql);
    transaction.commit();
}

} // namespace job_server::job_handlers

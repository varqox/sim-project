#include "reset_problem_time_limits.hh"

#include <sim/jobs/old_job.hh>
#include <sim/old_mysql/old_mysql.hh>
#include <simlib/file_remover.hh>
#include <simlib/sim/problem_package.hh>

using sim::internal_files::old_path_of;
using sim::jobs::OldJob;

namespace job_server::job_handlers {

void ResetProblemTimeLimits::run(sim::mysql::Connection& mysql) {
    STACK_UNWINDING_MARK;

    uint64_t problem_file_id = 0;
    {
        auto old_mysql = old_mysql::ConnectionView{mysql};
        auto stmt = old_mysql.prepare("SELECT file_id FROM problems WHERE id=?");
        stmt.bind_and_execute(problem_id_);
        stmt.res_bind_all(problem_file_id);
        if (not stmt.next()) {
            return set_failure("Problem with ID = ", problem_id_, " does not exist");
        }
    }

    auto pkg_path = sim::internal_files::old_path_of(problem_file_id);
    reset_package_time_limits(pkg_path);
    if (failed()) {
        return;
    }

    auto transaction = mysql.start_repeatable_read_transaction();
    auto old_mysql = old_mysql::ConnectionView{mysql};

    old_mysql.prepare("INSERT INTO internal_files (created_at) VALUES(?)")
        .bind_and_execute(utc_mysql_datetime());
    uint64_t new_file_id = old_mysql.insert_id();
    auto new_pkg_path = sim::internal_files::old_path_of(new_file_id);

    // Save Simfile to new package file

    ZipFile src_zip(pkg_path, ZIP_RDONLY);
    auto simfile_path = concat(sim::zip_package_main_dir(src_zip), "Simfile");

    FileRemover new_pkg_remover(new_pkg_path.to_string());
    ZipFile dest_zip(new_pkg_path, ZIP_CREATE | ZIP_TRUNCATE);

    auto eno = src_zip.entries_no();
    for (decltype(eno) i = 0; i < eno; ++i) {
        auto entry_name = src_zip.get_name(i);
        if (entry_name == simfile_path) {
            dest_zip.file_add(simfile_path, dest_zip.source_buffer(new_simfile_));
        } else {
            dest_zip.file_add(entry_name, dest_zip.source_zip(src_zip, i));
        }
    }

    dest_zip.close(); // Write all data to the dest_zip

    const auto current_date = utc_mysql_datetime();
    // Add job to delete old problem file
    old_mysql
        .prepare("INSERT INTO jobs(creator, type, priority, status, created_at, aux_id) "
                 "SELECT NULL, ?, ?, ?, ?, file_id FROM problems "
                 "WHERE id=?")
        .bind_and_execute(
            EnumVal(OldJob::Type::DELETE_FILE),
            default_priority(OldJob::Type::DELETE_FILE),
            EnumVal(OldJob::Status::PENDING),
            current_date,
            problem_id_
        );

    // Use new package as problem file
    old_mysql
        .prepare("UPDATE problems SET file_id=?, simfile=?, updated_at=? "
                 "WHERE id=?")
        .bind_and_execute(new_file_id, new_simfile_, current_date, problem_id_);

    job_done(mysql);

    transaction.commit();
    new_pkg_remover.cancel();
}

} // namespace job_server::job_handlers

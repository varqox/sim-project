#include "change_problem_statement.hh"

#include <simlib/file_manip.hh>
#include <simlib/file_remover.hh>
#include <simlib/macros/wont_throw.hh>
#include <simlib/path.hh>
#include <simlib/sim/problem_package.hh>
#include <simlib/sim/simfile.hh>
#include <simlib/time.hh>

using sim::jobs::OldJob;

namespace job_server::job_handlers {

void ChangeProblemStatement::run(sim::mysql::Connection& mysql) {
    STACK_UNWINDING_MARK;

    auto transaction = mysql.start_repeatable_read_transaction();
    auto old_mysql = old_mysql::ConnectionView{mysql};

    uint64_t problem_file_id = 0;
    sim::Simfile simfile;
    {
        auto stmt = old_mysql.prepare("SELECT file_id, simfile FROM problems"
                                      " WHERE id=?");
        stmt.bind_and_execute(problem_id_);
        InplaceBuff<0> simfile_str;
        stmt.res_bind_all(problem_file_id, simfile_str);
        if (not stmt.next()) {
            return set_failure("Problem with ID = ", problem_id_, " does not exist");
        }

        simfile = sim::Simfile(simfile_str.to_string());
        simfile.load_all();
    }

    auto pkg_path = sim::internal_files::old_path_of(problem_file_id);

    old_mysql.prepare("INSERT INTO internal_files (created_at) VALUES(?)")
        .bind_and_execute(mysql_date());
    uint64_t new_file_id = old_mysql.insert_id();
    auto new_pkg_path = sim::internal_files::old_path_of(new_file_id);

    // Replace old statement with new statement

    // Escape info.new_statement_path
    if (info_.new_statement_path.empty()) {
        info_.new_statement_path = WONT_THROW(simfile.statement.value());
    } else {
        info_.new_statement_path = path_absolute(info_.new_statement_path).erase(0, 1);
    }

    ZipFile src_zip(pkg_path, ZIP_RDONLY);
    auto main_dir = sim::zip_package_main_dir(src_zip);
    auto old_statement_path = concat(main_dir, WONT_THROW(simfile.statement.value()));
    auto new_statement_path = concat(main_dir, info_.new_statement_path);
    auto simfile_path = concat(main_dir, "Simfile");
    if (new_statement_path == simfile_path) {
        return set_failure("Invalid new statement path - it would overwrite Simfile");
    }

    simfile.statement = info_.new_statement_path;
    auto simfile_str = simfile.dump();

    FileRemover new_pkg_remover(new_pkg_path.to_string());
    ZipFile dest_zip(new_pkg_path, ZIP_CREATE | ZIP_TRUNCATE);

    auto eno = src_zip.entries_no();
    for (decltype(eno) i = 0; i < eno; ++i) {
        auto entry_name = src_zip.get_name(i);
        if (entry_name == simfile_path) {
            dest_zip.file_add(simfile_path, dest_zip.source_buffer(simfile_str));
        } else if (entry_name != old_statement_path) {
            dest_zip.file_add(entry_name, dest_zip.source_zip(src_zip, i));
        }
    }

    // Add new statement file entry
    dest_zip.file_add(
        new_statement_path, dest_zip.source_file(sim::internal_files::old_path_of(job_file_id_))
    );

    dest_zip.close(); // Write all data to the dest_zip

    const auto current_date = mysql_date();
    // Add job to delete old problem file
    old_mysql
        .prepare("INSERT INTO jobs(file_id, creator, type, priority, status,"
                 " created_at, aux_id, info, data)"
                 " SELECT file_id, NULL, ?, ?, ?, ?, NULL, '', '' FROM problems"
                 " WHERE id=?")
        .bind_and_execute(
            EnumVal(OldJob::Type::DELETE_FILE),
            default_priority(OldJob::Type::DELETE_FILE),
            EnumVal(OldJob::Status::PENDING),
            current_date,
            problem_id_
        );

    // Use new package as problem file
    old_mysql
        .prepare("UPDATE problems SET file_id=?, simfile=?, updated_at=?"
                 " WHERE id=?")
        .bind_and_execute(new_file_id, simfile_str, current_date, problem_id_);

    job_done(mysql, from_unsafe{info_.dump()});

    transaction.commit();
    new_pkg_remover.cancel();
}

} // namespace job_server::job_handlers

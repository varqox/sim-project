#include "change_problem_statement.hh"

#include <sim/change_problem_statement_jobs/change_problem_statement_job.hh>
#include <sim/internal_files/internal_file.hh>
#include <sim/problems/problem.hh>
#include <sim/sql/sql.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/file_remover.hh>
#include <simlib/libzip.hh>
#include <simlib/macros/wont_throw.hh>
#include <simlib/path.hh>
#include <simlib/sim/problem_package.hh>
#include <simlib/sim/simfile.hh>
#include <simlib/time.hh>
#include <zip.h>

using sim::change_problem_statement_jobs::ChangeProblemStatementJob;
using sim::jobs::Job;
using sim::problems::Problem;
using sim::sql::InsertInto;
using sim::sql::Select;
using sim::sql::Update;

namespace job_server::job_handlers {

void ChangeProblemStatement::run(sim::mysql::Connection& mysql) {
    STACK_UNWINDING_MARK;

    auto transaction = mysql.start_repeatable_read_transaction();
    decltype(Job::aux_id)::value_type problem_id;
    decltype(ChangeProblemStatementJob::new_statement_file_id) new_statement_file_id;
    decltype(ChangeProblemStatementJob::path_for_new_statement) path_for_new_statement;
    {
        auto stmt =
            mysql.execute(Select("j.aux_id, cpsj.new_statement_file_id, cpsj.path_for_new_statement"
            )
                              .from("jobs j")
                              .inner_join("change_problem_statement_jobs cpsj")
                              .on("cpsj.id=j.id")
                              .where("j.id=?", job_id_));
        stmt.res_bind(problem_id, new_statement_file_id, path_for_new_statement);
        throw_assert(stmt.next());
    }

    std::string problem_package_path;
    sim::Simfile simfile;
    {
        decltype(Problem::file_id) problem_file_id;
        decltype(Problem::simfile) simfile_as_str;
        auto stmt =
            mysql.execute(Select("file_id, simfile").from("problems").where("id=?", problem_id));
        stmt.res_bind(problem_file_id, simfile_as_str);
        if (!stmt.next()) {
            return set_failure("Problem with ID = ", problem_id, " does not exist");
        }

        problem_package_path = sim::internal_files::path_of(problem_file_id);
        simfile = sim::Simfile{simfile_as_str};
        simfile.load_all();
    }

    auto new_problem_file_id = sim::internal_files::new_internal_file_id(mysql);
    auto new_problem_package_path = sim::internal_files::path_of(new_problem_file_id);

    // Escape path_for_new_statement
    if (path_for_new_statement.empty()) {
        path_for_new_statement = WONT_THROW(simfile.statement.value());
    } else {
        path_for_new_statement = path_absolute(path_for_new_statement).erase(0, 1);
    }

    // Replace old statement with the new statement
    auto src_zip = ZipFile{problem_package_path, ZIP_RDONLY};
    auto main_dir = sim::zip_package_main_dir(src_zip);

    auto old_statement_path = concat_tostr(main_dir, WONT_THROW(simfile.statement.value()));
    auto new_statement_path = concat_tostr(main_dir, path_for_new_statement);
    auto simfile_path = concat_tostr(main_dir, "Simfile");
    if (new_statement_path == simfile_path) {
        return set_failure("Invalid new statement path - it would overwrite Simfile");
    }
    simfile.statement = path_for_new_statement;

    // Copy old package contests and update the Simfile
    auto simfile_as_str = simfile.dump();
    auto new_problem_package_remover = FileRemover{new_problem_package_path};
    auto dest_zip = ZipFile{new_problem_package_path, ZIP_CREATE | ZIP_TRUNCATE};
    auto eno = src_zip.entries_no();
    for (decltype(eno) i = 0; i < eno; ++i) {
        auto entry_name = src_zip.get_name(i);
        if (entry_name == simfile_path) {
            dest_zip.file_add(simfile_path, dest_zip.source_buffer(simfile_as_str));
        } else if (entry_name != old_statement_path && entry_name != new_statement_path) {
            dest_zip.file_add(entry_name, dest_zip.source_zip(src_zip, i));
        }
    }
    // Add new statement file entry
    dest_zip.file_add(
        new_statement_path,
        dest_zip.source_file(sim::internal_files::path_of(new_statement_file_id))
    );
    // Save all data and close the new problem package
    dest_zip.close();

    // Add job to delete the old problem file
    const auto current_datetime = utc_mysql_datetime();
    mysql.execute(
        InsertInto("jobs (file_id, creator, type, priority, status, created_at, aux_id, data)")
            .select(
                "file_id, NULL, ?, ?, ?, ?, NULL, ''",
                Job::Type::DELETE_FILE,
                default_priority(Job::Type::DELETE_FILE),
                Job::Status::PENDING,
                current_datetime
            )
            .from("problems")
            .where("id=?", problem_id)
    );

    // Update the problem with new file and simfile
    mysql.execute(Update("problems")
                      .set(
                          "file_id=?, simfile=?, updated_at=?",
                          new_problem_file_id,
                          simfile_as_str,
                          current_datetime
                      )
                      .where("id=?", problem_id));

    job_done(mysql);
    transaction.commit();
    new_problem_package_remover.cancel();
}

} // namespace job_server::job_handlers

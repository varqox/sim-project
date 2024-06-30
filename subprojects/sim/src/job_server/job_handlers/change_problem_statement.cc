#include "change_problem_statement.hh"
#include "common.hh"

#include <sim/change_problem_statement_jobs/change_problem_statement_job.hh>
#include <sim/internal_files/internal_file.hh>
#include <sim/jobs/job.hh>
#include <sim/mysql/mysql.hh>
#include <sim/problems/problem.hh>
#include <sim/sql/sql.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/file_remover.hh>
#include <simlib/libzip.hh>
#include <simlib/macros/stack_unwinding.hh>
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

void change_problem_statement(
    sim::mysql::Connection& mysql,
    Logger& logger,
    decltype(Job::id) job_id,
    decltype(Problem::id) problem_id
) {
    STACK_UNWINDING_MARK;

    auto transaction = mysql.start_repeatable_read_transaction();
    decltype(ChangeProblemStatementJob::new_statement_file_id) new_statement_file_id;
    decltype(ChangeProblemStatementJob::path_for_new_statement) path_for_new_statement;
    {
        auto stmt = mysql.execute(Select("new_statement_file_id, path_for_new_statement")
                                      .from("change_problem_statement_jobs")
                                      .where("id=?", job_id));
        stmt.res_bind(new_statement_file_id, path_for_new_statement);
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
            logger("The problem with id: ", problem_id, " does not exist");
            mark_job_as_cancelled(mysql, logger, job_id);
            transaction.commit();
            return;
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
        logger("Invalid new statement path - it would overwrite Simfile");
        mark_job_as_cancelled(mysql, logger, job_id);
        transaction.commit();
        return;
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
    mysql.execute(InsertInto("jobs (created_at, creator, type, priority, status, aux_id)")
                      .select(
                          "?, NULL, ?, ?, ?, file_id",
                          current_datetime,
                          Job::Type::DELETE_INTERNAL_FILE,
                          default_priority(Job::Type::DELETE_INTERNAL_FILE),
                          Job::Status::PENDING
                      )
                      .from("problems")
                      .where("id=?", problem_id));

    // Update the problem with new file and simfile
    mysql.execute(Update("problems")
                      .set(
                          "file_id=?, simfile=?, updated_at=?",
                          new_problem_file_id,
                          simfile_as_str,
                          current_datetime
                      )
                      .where("id=?", problem_id));

    mark_job_as_done(mysql, logger, job_id);
    transaction.commit();
    new_problem_package_remover.cancel();
}

} // namespace job_server::job_handlers

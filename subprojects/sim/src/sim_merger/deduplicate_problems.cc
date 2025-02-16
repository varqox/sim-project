#include "deduplicate_problems.hh"
#include "progress_printer.hh"
#include "sim_instance.hh"

#include <sim/internal_files/internal_file.hh>
#include <sim/jobs/job.hh>
#include <sim/problems/problem.hh>
#include <sim/sql/sql.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/libzip.hh>
#include <simlib/logger.hh>
#include <simlib/macros/stack_unwinding.hh>
#include <simlib/sha.hh>
#include <simlib/sim/problem_package.hh>
#include <simlib/sim/simfile.hh>
#include <simlib/time.hh>
#include <string>
#include <unordered_map>
#include <zip.h>

using sim::jobs::Job;
using sim::problems::Problem;
using sim::sql::InsertInto;
using sim::sql::Select;

void deduplicate_problems(const SimInstance& main_sim) {
    STACK_UNWINDING_MARK;

    auto merge_checking_problem_progress_printer =
        ProgressPrinter<decltype(Problem::id)>{"progress: merge-checking problem with id:"};
    auto stmt =
        main_sim.mysql.execute(Select("id, file_id, simfile").from("problems").order_by("id"));
    decltype(Problem::id) problem_id;
    decltype(Problem::file_id) problem_file_id;
    decltype(Problem::simfile) problem_simfile;
    stmt.res_bind(problem_id, problem_file_id, problem_simfile);

    std::unordered_map<std::string, decltype(Problem::id)> statement_hash_to_problem_id;
    while (stmt.next()) {
        merge_checking_problem_progress_printer.note_id(problem_id);

        // Obtain the statement
        auto simfile = sim::Simfile{problem_simfile};
        simfile.load_statement();
        const auto& statement_path = simfile.statement.value();

        auto zip_path = concat_tostr(main_sim.path, sim::internal_files::path_of(problem_file_id));
        auto zip = ZipFile{zip_path, ZIP_RDONLY};
        auto main_dir = sim::zip_package_main_dir(zip);

        auto statement_full_path = concat_tostr(main_dir, statement_path);
        auto statement = zip.extract_to_str(zip.get_index(statement_full_path));

        auto hash = sha3_512(statement).to_string();

        // Check if problem has a duplicate
        auto [it, inserted] = statement_hash_to_problem_id.try_emplace(hash, problem_id);
        if (inserted) {
            continue;
        }

        // Schedule merging of the problem into the found duplicate
        stdlog("scheduling problems merge: ", problem_id, " -> ", it->second);
        auto insert_stmt = main_sim.mysql.execute(
            InsertInto("jobs (created_at, creator, type, priority, status, aux_id, aux_id_2)")
                .values(
                    "?, NULL, ?, ?, ?, ?, ?",
                    utc_mysql_datetime(),
                    Job::Type::MERGE_PROBLEMS,
                    default_priority(Job::Type::MERGE_PROBLEMS),
                    Job::Status::PENDING,
                    problem_id,
                    it->second
                )
        );
        auto job_id = insert_stmt.insert_id();
        main_sim.mysql.execute(
            InsertInto("merge_problems_jobs (id, rejudge_transferred_submissions)")
                .values("?, ?", job_id, false)
        );
    }
}

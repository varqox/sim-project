#include "src/job_server/job_handlers/reupload_problem.hh"
#include "src/job_server/main.hh"

namespace job_handlers {

void ReuploadProblem::run() {
    STACK_UNWINDING_MARK;

    auto transaction = mysql.start_read_committed_transaction();
    // Need to create the package
    if (not tmp_file_id_.has_value()) {
        build_package();
        if (failed()) {
            return;
        }

        if (need_main_solution_judge_report_) {
            bool canceled = false;
            job_done(canceled);
            if (not canceled) {
                transaction.commit();
                package_file_remover_.cancel();
                return;
            }
        }
    }

    replace_problem_in_db();

    submit_solutions();

    bool canceled = false;
    job_done(canceled);

    if (not failed() and not canceled) {
        transaction.commit();
        package_file_remover_.cancel();
        return;
    }
}

} // namespace job_handlers

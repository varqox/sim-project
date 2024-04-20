#include "reupload_problem.hh"

namespace job_server::job_handlers {

void ReuploadProblem::run(sim::mysql::Connection& mysql) {
    STACK_UNWINDING_MARK;

    auto transaction = mysql.start_read_committed_transaction();
    // Need to create the package
    if (not tmp_file_id_.has_value()) {
        build_package(mysql);
        if (failed()) {
            return;
        }

        if (need_main_solution_judge_report_) {
            bool canceled = false;
            job_done(mysql, canceled);
            if (not canceled) {
                transaction.commit();
                package_file_remover_.cancel();
                return;
            }
        }
    }

    replace_problem_in_db(mysql);

    submit_solutions(mysql);

    bool canceled = false;
    job_done(mysql, canceled);

    if (not failed() and not canceled) {
        transaction.commit();
        package_file_remover_.cancel();
        return;
    }
}

} // namespace job_server::job_handlers

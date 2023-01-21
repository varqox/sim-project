#include "reset_time_limits_in_problem_package_base.hh"
#include "../main.hh"

#include <sim/judging_config.hh>
#include <simlib/sim/problem_package.hh>

namespace job_server::job_handlers {

void ResetTimeLimitsInProblemPackageBase::reset_package_time_limits(FilePath package_path) {
    STACK_UNWINDING_MARK;

    job_log("Judging the model solution...");

    load_problem_package(package_path);

    auto& simfile = jworker_.simfile();
    simfile.load_all(); // Everything is needed as we will dump it to Simfile file
    job_log("Model solution: ", simfile.solutions[0]);

    auto compilation_errors = compile_solution_from_problem_package(
            simfile.solutions[0], sim::filename_to_lang(simfile.solutions[0]));
    if (compilation_errors.has_value()) {
        return set_failure();
    }

    compilation_errors = compile_checker();
    if (compilation_errors.has_value()) {
        return set_failure();
    }

    job_log("Judging...");

    sim::VerboseJudgeLogger logger(true);
    sim::JudgeReport initial_rep = jworker_.judge(false, logger);
    job_log("Initial judge report: ", initial_rep.judge_log);

    sim::JudgeReport final_rep = jworker_.judge(true, logger);
    job_log("Final judge report: ", final_rep.judge_log);

    try {
        sim::Conver::ResetTimeLimitsOptions opts;
        opts.min_time_limit = sim::MIN_TIME_LIMIT;
        opts.solution_runtime_coefficient = sim::SOLUTION_RUNTIME_COEFFICIENT;

        sim::Conver::reset_time_limits_using_jugde_reports(simfile, initial_rep, final_rep, opts);

    } catch (const std::exception& e) {
        return set_failure("Conver failed: ", e.what());
    }

    new_simfile_ = simfile.dump();
}

} // namespace job_server::job_handlers

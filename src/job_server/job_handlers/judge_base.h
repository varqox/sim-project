#pragma once

#include "job_handler.h"

#include <sim/constants.h>
#include <simlib/sim/judge_worker.h>

namespace job_handlers {

class JudgeBase : virtual public JobHandler {
protected:
	sim::JudgeWorker jworker_;

	JudgeBase();

	static sim::SolutionLanguage to_sol_lang(SubmissionLanguage lang);

	// Creates an xml report from JudgeReport
	InplaceBuff<65536> construct_report(const sim::JudgeReport& jr, bool final);

	// Returns OK or the first encountered error status
	SubmissionStatus calc_status(const sim::JudgeReport& jr);

	void load_problem_package(FilePath problem_pkg_path);

private:
	// Iff compilation failed, compilation errors are returned
	template <class MethodPtr>
	std::optional<std::string> compile_solution_impl(FilePath solution_path,
	                                                 sim::SolutionLanguage lang,
	                                                 MethodPtr compile_method);

protected:
	// Iff compilation failed, compilation errors are returned
	std::optional<std::string> compile_solution(FilePath solution_path,
	                                            sim::SolutionLanguage lang);

	// Iff compilation failed, compilation errors are returned
	std::optional<std::string>
	compile_solution_from_problem_package(FilePath solution_path,
	                                      sim::SolutionLanguage lang);

	std::optional<std::string> compile_checker();

public:
	virtual ~JudgeBase() = default;
};

} // namespace job_handlers

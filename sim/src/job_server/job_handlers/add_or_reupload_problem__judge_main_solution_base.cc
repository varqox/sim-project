#include "add_or_reupload_problem__judge_main_solution_base.hh"

#include <simlib/sim/problem_package.hh>

namespace job_handlers {

void AddOrReuploadProblemJudgeModelSolutionBase::run() {
	STACK_UNWINDING_MARK;

	auto package_path = internal_file_path(tmp_file_id_.value());
	reset_package_time_limits(package_path);
	if (failed())
		return;

	// Put the Simfile in the package
	ZipFile zip(package_path);
	zip.file_add(concat(sim::zip_package_main_dir(zip), "Simfile"),
	             zip.source_buffer(new_simfile_), ZIP_FL_OVERWRITE);
	zip.close();

	bool canceled;
	job_done(canceled);
}

} // namespace job_handlers

#include "problem_add_or_reupload_judge_model_solution_job_handler_base.h"

#include <simlib/sim/problem_package.h>

void ProblemAddOrReuploadJudgeModelSolutionJobHandlerBase::run() {
	STACK_UNWINDING_MARK;

	auto package_path = internal_file_path(tmp_file_id.value());
	reset_package_time_limits(package_path);
	if (failed())
		return;

	// Put the Simfile in the package
	ZipFile zip(package_path);
	zip.file_add(concat(sim::zip_package_master_dir(zip), "Simfile"),
	             zip.source_buffer(new_simfile), ZIP_FL_OVERWRITE);
	zip.close();

	bool canceled;
	job_done(canceled);
}

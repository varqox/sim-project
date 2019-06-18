#pragma once

#include "job_handler.h"

#include <sim/jobs.h>
#include <simlib/libzip.h>

class ProblemAddOrReuploadJobHandlerBase : virtual public JobHandler {
protected:
	JobType job_type;
	StringView job_creator;
	jobs::AddProblemInfo info;
	FileRemover package_file_remover;
	uint64_t job_file_id;
	Optional<uint64_t> tmp_file_id;
	Optional<uint64_t> problem_id;
	// Internal state
	bool need_model_solution_judge_report = false;

private:
	bool replace_db_job_log = false;
	ZipFile zip;
	std::string master_dir;
	std::string current_date;
	std::string simfile_str;
	sim::Simfile simfile;

	void load_job_log_from_DB();

protected:
	ProblemAddOrReuploadJobHandlerBase(JobType job_type, StringView job_creator,
	                                   const jobs::AddProblemInfo& info,
	                                   uint64_t job_file_id,
	                                   Optional<uint64_t> tmp_file_id,
	                                   Optional<uint64_t> problem_id)
	   : job_type(job_type), job_creator(job_creator), info(info),
	     job_file_id(job_file_id), tmp_file_id(tmp_file_id),
	     problem_id(problem_id) {
		if (tmp_file_id.has_value())
			load_job_log_from_DB();
	}

	static void assert_transaction_is_open();

	// Runs conver and places package into a temporary internal file. Sets
	// need_model_solution_judge_report
	void build_package();

private:
	// Initializes internal variables for use by the next functions
	void open_package();

protected:
	void add_problem_to_DB();

	void replace_problem_in_DB();

	void submit_solutions();

	using JobHandler::job_done;

	void job_done(bool& job_was_canceled);

public:
	virtual ~ProblemAddOrReuploadJobHandlerBase() = default;
};

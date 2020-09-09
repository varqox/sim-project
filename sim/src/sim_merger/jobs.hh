#pragma once

#include "internal_files.hh"
#include "sim/constants.hh"
#include "sim/jobs.hh"
#include "submissions.hh"
#include "users.hh"

struct Job {
	uintmax_t id;
	std::optional<uintmax_t> file_id;
	std::optional<uintmax_t> tmp_file_id;
	std::optional<uintmax_t> creator;
	EnumVal<JobType> type;
	uintmax_t priority;
	EnumVal<JobStatus> status;
	InplaceBuff<24> added;
	std::optional<uintmax_t> aux_id;
	InplaceBuff<128> info;
	InplaceBuff<1> data;
};

class JobsMerger : public Merger<Job> {
	const InternalFilesMerger& internal_files_;
	const UsersMerger& users_;
	const SubmissionsMerger& submissions_;
	const ProblemsMerger& problems_;
	const ContestsMerger& contests_;
	const ContestRoundsMerger& contest_rounds_;
	const ContestProblemsMerger& contest_problems_;

	void load(RecordSet& record_set) override {
		STACK_UNWINDING_MARK;

		Job job;
		MySQL::Optional<uintmax_t> m_file_id;
		MySQL::Optional<uintmax_t> m_tmp_file_id;
		MySQL::Optional<uintmax_t> m_creator;
		MySQL::Optional<uintmax_t> m_aux_id;
		auto stmt =
		   conn.prepare("SELECT id, file_id, tmp_file_id, creator, type,"
		                " priority, status, added, aux_id, info, data "
		                "FROM ",
		                record_set.sql_table_name);
		stmt.bind_and_execute();
		stmt.res_bind_all(job.id, m_file_id, m_tmp_file_id, m_creator, job.type,
		                  job.priority, job.status, job.added, m_aux_id,
		                  job.info, job.data);
		while (stmt.next()) {
			job.file_id = m_file_id.opt();
			job.tmp_file_id = m_tmp_file_id.opt();
			job.creator = m_creator.opt();
			job.aux_id = m_aux_id.opt();

			if (job.file_id) {
				job.file_id =
				   internal_files_.new_id(job.file_id.value(), record_set.kind);
			}
			if (job.tmp_file_id) {
				job.tmp_file_id = internal_files_.new_id(
				   job.tmp_file_id.value(), record_set.kind);
			}
			if (job.creator) {
				job.creator =
				   users_.new_id(job.creator.value(), record_set.kind);
			}

			// Process type-specific ids
			switch (job.type) {
			case JobType::DELETE_FILE:
				// Id is already processed
				break;

			case JobType::JUDGE_SUBMISSION:
			case JobType::REJUDGE_SUBMISSION:
				job.aux_id =
				   submissions_.new_id(job.aux_id.value(), record_set.kind);
				break;

			case JobType::DELETE_PROBLEM:
			case JobType::REUPLOAD_PROBLEM:
			case JobType::REUPLOAD_PROBLEM__JUDGE_MODEL_SOLUTION:
			case JobType::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
			case JobType::CHANGE_PROBLEM_STATEMENT:
				job.aux_id =
				   problems_.new_id(job.aux_id.value(), record_set.kind);
				break;

			case JobType::MERGE_PROBLEMS: {
				job.aux_id =
				   problems_.new_id(job.aux_id.value(), record_set.kind);
				auto info = jobs::MergeProblemsInfo(job.info);
				info.target_problem_id =
				   problems_.new_id(info.target_problem_id, record_set.kind);
				job.info = info.dump();
				break;
			}

			case JobType::MERGE_USERS: {
				job.aux_id = users_.new_id(job.aux_id.value(), record_set.kind);
				auto info = jobs::MergeUsersInfo(job.info);
				info.target_user_id =
				   users_.new_id(info.target_user_id, record_set.kind);
				job.info = info.dump();
				break;
			}

			case JobType::DELETE_USER:
				job.aux_id = users_.new_id(job.aux_id.value(), record_set.kind);
				break;

			case JobType::DELETE_CONTEST:
				job.aux_id =
				   contests_.new_id(job.aux_id.value(), record_set.kind);
				break;

			case JobType::DELETE_CONTEST_ROUND:
				job.aux_id =
				   contest_rounds_.new_id(job.aux_id.value(), record_set.kind);
				break;

			case JobType::DELETE_CONTEST_PROBLEM:
			case JobType::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
				job.aux_id = contest_problems_.new_id(job.aux_id.value(),
				                                      record_set.kind);
				break;

			case JobType::ADD_PROBLEM:
			case JobType::ADD_PROBLEM__JUDGE_MODEL_SOLUTION:
				if (job.aux_id) {
					job.aux_id =
					   problems_.new_id(job.aux_id.value(), record_set.kind);
				}
				break;

			case JobType::EDIT_PROBLEM: THROW("TODO");
			}

			record_set.add_record(
			   job, str_to_time_point(
			           intentional_unsafe_cstring_view(job.added.to_string())));
		}
	}

	void merge() override {
		STACK_UNWINDING_MARK;
		Merger::merge([&](const Job&) { return nullptr; });
	}

public:
	void save_merged() override {
		STACK_UNWINDING_MARK;
		auto transaction = conn.start_transaction();
		conn.update("TRUNCATE ", sql_table_name());
		auto stmt =
		   conn.prepare("INSERT INTO ", sql_table_name(),
		                "(id, file_id, tmp_file_id, creator, type, priority,"
		                " status, added, aux_id, info, data) "
		                "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

		ProgressBar progress_bar("Jobs saved:", new_table_.size(), 128);
		for (const NewRecord& new_record : new_table_) {
			Defer progressor = [&] { progress_bar.iter(); };
			const Job& x = new_record.data;
			stmt.bind_and_execute(x.id, x.file_id, x.tmp_file_id, x.creator,
			                      x.type, x.priority, x.status, x.added,
			                      x.aux_id, x.info, x.data);
		}

		conn.update("ALTER TABLE ", sql_table_name(),
		            " AUTO_INCREMENT=", last_new_id_ + 1);
		transaction.commit();
	}

	JobsMerger(const IdsFromMainAndOtherJobs& ids_from_both_jobs,
	           const InternalFilesMerger& internal_files,
	           const UsersMerger& users, const SubmissionsMerger& submissions,
	           const ProblemsMerger& problems, const ContestsMerger& contests,
	           const ContestRoundsMerger& contest_rounds,
	           const ContestProblemsMerger& contest_problems)
	: Merger("jobs", ids_from_both_jobs.main.jobs,
	         ids_from_both_jobs.other.jobs)
	, internal_files_(internal_files)
	, users_(users)
	, submissions_(submissions)
	, problems_(problems)
	, contests_(contests)
	, contest_rounds_(contest_rounds)
	, contest_problems_(contest_problems) {
		STACK_UNWINDING_MARK;
		initialize();
	}
};

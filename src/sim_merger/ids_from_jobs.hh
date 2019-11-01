#pragma once

#include "sim_merger.hh"

#include <chrono>
#include <map>
#include <sim/constants.h>
#include <sim/jobs.h>
#include <simlib/time.h>

// For each id, we keep the first date that it appeared with
template <class IdType>
struct IdsWithTime {
	std::map<IdType, std::chrono::system_clock::time_point> ids;

	void add_id(IdType id, std::chrono::system_clock::time_point tp) {
		auto it = ids.find(id);
		if (it == ids.end()) {
			ids.emplace(id, tp);
			return;
		}

		if (it->second > tp)
			it->second = tp;
	}

	void normalize_times() {
		if (ids.empty())
			return;

		auto last_tp = (--ids.end())->second;
		for (auto& [id, tp] : reverse_view(ids)) {
			if (tp > last_tp)
				tp = last_tp;
			else
				last_tp = tp;
		}
	}
};

struct ProblemTagId {
	uintmax_t problem_id;
	InplaceBuff<PROBLEM_TAG_MAX_LEN> tag;
};

bool operator<(const ProblemTagId& a, const ProblemTagId& b) noexcept {
	return std::pair(a.problem_id, a.tag) < std::pair(b.problem_id, b.tag);
}

inline auto stringify(ProblemTagId ptag) {
	return concat("(problem: ", ptag.problem_id, " tag: ", ptag.tag, ')');
}

struct ContestUserId {
	uintmax_t user_id;
	uintmax_t contest_id;
};

bool operator<(const ContestUserId& a, const ContestUserId& b) noexcept {
	return std::pair(a.user_id, a.contest_id) <
	       std::pair(b.user_id, b.contest_id);
}

inline auto stringify(ContestUserId cuser) {
	return concat("(user: ", cuser.user_id, " contest: ", cuser.contest_id,
	              ')');
}

// Loads all information about ids from jobs
struct IdsFromJobs {
	IdsWithTime<uintmax_t> internal_files;
	IdsWithTime<uintmax_t> users;
	IdsWithTime<InplaceBuff<SESSION_ID_LEN>> sessions;
	IdsWithTime<uintmax_t> problems;
	IdsWithTime<ProblemTagId> problem_tags;
	IdsWithTime<uintmax_t> contests;
	IdsWithTime<uintmax_t> contest_rounds;
	IdsWithTime<uintmax_t> contest_problems;
	IdsWithTime<ContestUserId> contest_users;
	IdsWithTime<InplaceBuff<FILE_ID_LEN>> contest_files;
	IdsWithTime<InplaceBuff<CONTEST_ENTRY_TOKEN_LEN>> contest_entry_tokens;
	IdsWithTime<uintmax_t> submissions;
	IdsWithTime<uintmax_t> jobs;

	void initialize(StringView job_table_name) {
		STACK_UNWINDING_MARK;

		uintmax_t id;
		MySQL::Optional<uintmax_t> creator;
		EnumVal<JobType> type;
		MySQL::Optional<uintmax_t> file_id;
		MySQL::Optional<uintmax_t> tmp_file_id;
		InplaceBuff<24> added_str;
		MySQL::Optional<uintmax_t> aux_id;
		InplaceBuff<32> info;

		auto stmt = conn.prepare("SELECT id, creator, type, file_id, "
		                         "tmp_file_id, added, aux_id, info FROM ",
		                         job_table_name, " ORDER BY id");
		stmt.bindAndExecute();
		stmt.res_bind_all(id, creator, type, file_id, tmp_file_id, added_str,
		                  aux_id, info);
		while (stmt.next()) {
			auto added = strToTimePoint(added_str.to_string());

			// Process non-type-specific ids
			jobs.add_id(id, added);
			if (file_id.has_value())
				internal_files.add_id(file_id.value(), added);
			if (tmp_file_id.has_value())
				internal_files.add_id(tmp_file_id.value(), added);
			if (creator.has_value())
				users.add_id(creator.value(), added);

			// Process type-specific ids
			switch (type) {
			case JobType::DELETE_FILE:
				// Id is already processed
				break;

			case JobType::JUDGE_SUBMISSION:
			case JobType::REJUDGE_SUBMISSION:
				submissions.add_id(aux_id.value(), added);
				break;

			case JobType::DELETE_PROBLEM:
			case JobType::REUPLOAD_PROBLEM:
			case JobType::REUPLOAD_PROBLEM__JUDGE_MODEL_SOLUTION:
			case JobType::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
			case JobType::CHANGE_PROBLEM_STATEMENT:
				problems.add_id(aux_id.value(), added);
				break;

			case JobType::MERGE_PROBLEMS:
				problems.add_id(aux_id.value(), added);
				problems.add_id(jobs::MergeProblemsInfo(info).target_problem_id,
				                added);
				break;

			case JobType::DELETE_USER:
				users.add_id(aux_id.value(), added);
				break;

			case JobType::DELETE_CONTEST:
				contests.add_id(aux_id.value(), added);
				break;

			case JobType::DELETE_CONTEST_ROUND:
				contest_rounds.add_id(aux_id.value(), added);
				break;

			case JobType::DELETE_CONTEST_PROBLEM:
			case JobType::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
				contest_problems.add_id(aux_id.value(), added);
				break;

			case JobType::ADD_PROBLEM:
			case JobType::ADD_PROBLEM__JUDGE_MODEL_SOLUTION:
				if (aux_id.has_value())
					problems.add_id(aux_id.value(), added);
				break;

			case JobType::EDIT_PROBLEM: THROW("TODO");
			}
		}
	}

	explicit IdsFromJobs(StringView job_table_name) {
		STACK_UNWINDING_MARK;
		initialize(job_table_name);
	}
};

struct IdsFromMainAndOtherJobs {
	IdsFromJobs main {
	   intentionalUnsafeStringView(concat(main_sim_table_prefix, "jobs"))};
	IdsFromJobs other {"jobs"};
};

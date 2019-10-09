#include "sim.h"

#include <sim/jobs.h>

using std::string;

Sim::JobPermissions Sim::jobs_get_overall_permissions() noexcept {
	STACK_UNWINDING_MARK;
	using PERM = JobPermissions;

	if (not session_is_open)
		return PERM::NONE;

	switch (session_user_type) {
	case UserType::ADMIN: return PERM::VIEW_ALL;
	case UserType::TEACHER:
	case UserType::NORMAL: return PERM::NONE;
	}

	return PERM::NONE; // Shouldn't happen
}

Sim::JobPermissions
Sim::jobs_get_permissions(std::optional<StringView> creator_id,
                          JobType job_type, JobStatus job_status) noexcept {
	STACK_UNWINDING_MARK;
	using PERM = JobPermissions;
	using JT = JobType;
	using JS = JobStatus;

	JobPermissions overall_perms = jobs_get_overall_permissions();

	if (not session_is_open)
		return overall_perms;

	static_assert(uint(PERM::NONE) == 0, "Needed below");
	JobPermissions type_perm = [job_type] {
		switch (job_type) {
		case JT::ADD_PROBLEM:
		case JT::REUPLOAD_PROBLEM:
		case JT::ADD_PROBLEM__JUDGE_MODEL_SOLUTION:
		case JT::REUPLOAD_PROBLEM__JUDGE_MODEL_SOLUTION:
			return PERM::DOWNLOAD_LOG | PERM::DOWNLOAD_UPLOADED_PACKAGE;

		case JT::CHANGE_PROBLEM_STATEMENT:
			return PERM::DOWNLOAD_LOG | PERM::DOWNLOAD_UPLOADED_STATEMENT;

		case JT::JUDGE_SUBMISSION:
		case JT::REJUDGE_SUBMISSION:
		case JT::EDIT_PROBLEM:
		case JT::DELETE_PROBLEM:
		case JT::MERGE_PROBLEMS:
		case JT::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
		case JT::DELETE_USER:
		case JT::DELETE_CONTEST:
		case JT::DELETE_CONTEST_ROUND:
		case JT::DELETE_CONTEST_PROBLEM:
		case JT::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
		case JT::DELETE_FILE: return PERM::NONE;
		}

		return PERM::NONE; // Shouldn't happen
	}();

	if (session_user_type == UserType::ADMIN) {
		switch (job_status) {
		case JS::PENDING:
		case JS::NOTICED_PENDING:
		case JS::IN_PROGRESS:
			return overall_perms | type_perm | PERM::VIEW | PERM::DOWNLOAD_LOG |
			       PERM::VIEW_ALL | PERM::CANCEL;

		case JS::FAILED:
		case JS::CANCELED:
			return overall_perms | type_perm | PERM::VIEW | PERM::DOWNLOAD_LOG |
			       PERM::VIEW_ALL | PERM::RESTART;

		case JS::DONE:
			return overall_perms | type_perm | PERM::VIEW | PERM::DOWNLOAD_LOG |
			       PERM::VIEW_ALL;
		}
	}

	if (creator_id.has_value() and session_user_id == creator_id.value()) {
		if (is_one_of(job_status, JS::PENDING, JS::NOTICED_PENDING,
		              JS::IN_PROGRESS))
			return overall_perms | type_perm | PERM::VIEW | PERM::CANCEL;
		else
			return overall_perms | type_perm | PERM::VIEW;
	}

	return overall_perms;
}

Sim::JobPermissions
Sim::jobs_granted_permissions_problem(StringView problem_id) {
	STACK_UNWINDING_MARK;
	using PERM = JobPermissions;

	auto stmt = mysql.prepare("SELECT owner, type FROM problems WHERE id=?");
	stmt.bindAndExecute(problem_id);

	InplaceBuff<32> powner;
	EnumVal<ProblemType> ptype;
	stmt.res_bind_all(powner, ptype);
	if (stmt.next()) {
		auto pperms = problems_get_permissions(powner, ptype);
		if (uint(pperms & ProblemPermissions::VIEW_RELATED_JOBS))
			return PERM::VIEW | PERM::DOWNLOAD_LOG |
			       PERM::DOWNLOAD_UPLOADED_PACKAGE |
			       PERM::DOWNLOAD_UPLOADED_STATEMENT;
	}

	return PERM::NONE;
}

Sim::JobPermissions
Sim::jobs_granted_permissions_submission(StringView submission_id) {
	STACK_UNWINDING_MARK;
	using PERM = JobPermissions;
	using CUM = ContestUserMode;

	if (not session_is_open)
		return PERM::NONE;

	auto stmt = mysql.prepare("SELECT s.type, p.owner, p.type, cu.mode,"
	                          " c.is_public "
	                          "FROM submissions s "
	                          "STRAIGHT_JOIN problems p ON p.id=s.problem_id "
	                          "LEFT JOIN contests c ON c.id=s.contest_id "
	                          "LEFT JOIN contest_users cu ON cu.user_id=?"
	                          " AND cu.contest_id=s.contest_id "
	                          "WHERE s.id=?");
	stmt.bindAndExecute(session_user_id, submission_id);

	MySQL::Optional<unsigned char> is_public;
	InplaceBuff<32> powner;
	EnumVal<SubmissionType> stype;
	EnumVal<ProblemType> ptype;
	MySQL::Optional<EnumVal<CUM>> cu_mode;
	stmt.res_bind_all(stype, powner, ptype, cu_mode, is_public);
	if (stmt.next()) {
		if (is_public.has_value() and // <-- contest exists
		    uint(contests_get_permissions(is_public.value(), cu_mode) &
		         ContestPermissions::ADMIN)) {
			return PERM::VIEW | PERM::DOWNLOAD_LOG;
		}

		// The below check has to be done as the last one because it gives the
		// least permissions
		auto pperms = problems_get_permissions(powner, ptype);
		// Give access to the problem's submissions' jobs to the problem's admin
		if (bool(uint(pperms & ProblemPermissions::EDIT)))
			return PERM::VIEW | PERM::DOWNLOAD_LOG;
	}

	return PERM::NONE;
}

void Sim::jobs_handle() {
	STACK_UNWINDING_MARK;

	if (not session_is_open)
		return redirect("/login?" + request.target);

	StringView next_arg = url_args.extractNextArg();
	if (isDigit(next_arg)) {
		jobs_jid = next_arg;

		page_template(intentionalUnsafeStringView(concat("Job ", jobs_jid)),
		              "body{padding-left:20px}");
		append("<script>view_job(false, ", jobs_jid,
		       ", window.location.hash);</script>");
		return;
	}

	InplaceBuff<32> query_suffix {};
	if (next_arg == "my")
		query_suffix.append("/u", session_user_id);
	else if (next_arg.size())
		return error404();

	/* List jobs */
	page_template("Job queue", "body{padding-left:20px}");

	append("<h1>Jobs</h1>"
	       "<script>tab_jobs_lister($('body'));</script>");
}

#include "sim.h"

#include <sim/jobs.h>
#include <sim/problem_permissions.hh>

using sim::Problem;
using sim::User;
using std::optional;
using std::string;

Sim::JobPermissions Sim::jobs_get_overall_permissions() noexcept {
	STACK_UNWINDING_MARK;
	using PERM = JobPermissions;

	if (not session_is_open)
		return PERM::NONE;

	switch (session_user_type) {
	case User::Type::ADMIN: return PERM::VIEW_ALL;
	case User::Type::TEACHER:
	case User::Type::NORMAL: return PERM::NONE;
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
		case JT::MERGE_USERS:
		case JT::DELETE_USER:
		case JT::DELETE_CONTEST:
		case JT::DELETE_CONTEST_ROUND:
		case JT::DELETE_CONTEST_PROBLEM:
		case JT::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
		case JT::DELETE_FILE: return PERM::NONE;
		}

		return PERM::NONE; // Shouldn't happen
	}();

	if (session_user_type == User::Type::ADMIN) {
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
	using P_PERMS = sim::problem::Permissions;

	auto problem_perms =
	   sim::problem::get_permissions(
	      mysql, problem_id,
	      (session_is_open ? optional {WONT_THROW(
	                            str2num<uintmax_t>(session_user_id).value())}
	                       : std::nullopt),
	      (session_is_open ? optional {session_user_type} : std::nullopt))
	      .value_or(P_PERMS::NONE);

	if (uint(problem_perms & P_PERMS::VIEW_RELATED_JOBS)) {
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
	stmt.bind_and_execute(session_user_id, submission_id);

	EnumVal<SubmissionType> stype;
	MySQL::Optional<decltype(Problem::owner)::value_type> problem_owner;
	decltype(Problem::type) problem_type;
	MySQL::Optional<decltype(sim::ContestUser::mode)> cu_mode;
	MySQL::Optional<unsigned char> is_public;
	stmt.res_bind_all(stype, problem_owner, problem_type, cu_mode, is_public);
	if (stmt.next()) {
		if (is_public.has_value() and // <-- contest exists
		    uint(sim::contest::get_permissions(
		            (session_is_open ? std::optional {session_user_type}
		                             : std::nullopt),
		            is_public.value(), cu_mode) &
		         sim::contest::Permissions::ADMIN)) {
			return PERM::VIEW | PERM::DOWNLOAD_LOG;
		}

		// The below check has to be done as the last one because it gives the
		// least permissions
		auto problem_perms = sim::problem::get_permissions(
		   (session_is_open ? optional {WONT_THROW(
		                         str2num<uintmax_t>(session_user_id).value())}
		                    : std::nullopt),
		   (session_is_open ? optional {session_user_type} : std::nullopt),
		   problem_owner, problem_type);
		// Give access to the problem's submissions' jobs to the problem's admin
		if (bool(uint(problem_perms & sim::problem::Permissions::EDIT)))
			return PERM::VIEW | PERM::DOWNLOAD_LOG;
	}

	return PERM::NONE;
}

void Sim::jobs_handle() {
	STACK_UNWINDING_MARK;

	if (not session_is_open)
		return redirect("/login?" + request.target);

	StringView next_arg = url_args.extract_next_arg();
	if (is_digit(next_arg)) {
		jobs_jid = next_arg;

		page_template(intentional_unsafe_string_view(concat("Job ", jobs_jid)),
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

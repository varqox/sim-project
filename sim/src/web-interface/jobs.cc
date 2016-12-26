#include "form_validator.h"
#include "jobs.h"

#include <sim/jobs.h>

using std::string;

Jobs::Permissions Jobs::getPermissions(const string& owner_id) {
	if (Session::open()) {
		if (Session::user_type == UTYPE_ADMIN)
			return Permissions(PERM_VIEW | PERM_VIEW_ALL | PERM_CANCEL);

		if (Session::user_id == owner_id)
			return Permissions(PERM_VIEW | PERM_CANCEL |
				(Session::user_type == UTYPE_TEACHER ? (int)PERM_VIEW_ALL : 0));

		if (Session::user_type == UTYPE_TEACHER)
			return Permissions(PERM_VIEW_ALL);
	}

	return PERM_NONE;
}

static constexpr const char* job_type_str(JobQueueType type) {
	switch (type) {
	case JobQueueType::JUDGE_SUBMISSION: return "Judge submission";
	case JobQueueType::ADD_PROBLEM: return "Add problem";
	case JobQueueType::REUPLOAD_PROBLEM: return "Reupload problem";
	case JobQueueType::JUDGE_MODEL_SOLUTION: return "Judge model solution";
	case JobQueueType::EDIT_PROBLEM: return "Edit problem";
	case JobQueueType::DELETE_PROBLEM: return "Delete problem";
	}
	return "Unknown";
}

static constexpr const char* job_status_as_td(JobQueueStatus type) {
	switch (type) {
	case JobQueueStatus::VOID: return "<td class=\"status\">Unknown</td>";
	case JobQueueStatus::PENDING: return "<td class=\"status\">Pending</td>";
	case JobQueueStatus::IN_PROGRESS:
		return "<td class=\"status yellow\">In progress</td>";
	case JobQueueStatus::DONE: return "<td class=\"status green\">Done</td>";
	case JobQueueStatus::FAILED: return "<td class=\"status red\">Failed</td>";
	case JobQueueStatus::CANCELLED:
		return "<td class=\"status blue\">Cancelled</td>";
	}
	return "<td class=\"status\">Unknown</td>";
}

void Jobs::handle() {
	StringView next_arg = url_args.extractNextArg();
	if (isDigit(next_arg)) {
		job_id = next_arg.to_string();
		return job();
	}

	job_id.clear();
	perms = getPermissions(""); // Get permissions to overall job queue

	if (next_arg == "cancel")
		return cancelJob();

	else if (!next_arg.empty() && next_arg != "my")
		return error404();

	bool show_all = (next_arg != "my");
	if (show_all && ~perms & PERM_VIEW_ALL)
		return error403();

	jobsTemplate("Job queue");
	append(next_arg.empty() ? "<h1>Job queue</h1>" : "<h1>My jobs</h1>");

	// List available problems
	try {
		DB::Statement stmt;
		if (show_all) {
			stmt = db_conn.prepare("SELECT j.id, added, j.type, status,"
				" priority, aux_id, info, creator, username"
				" FROM job_queue j, users u"
				" WHERE creator=u.id ORDER BY id DESC");

		} else {
			stmt = db_conn.prepare("SELECT id, added, type, status, priority,"
				" aux_id, info"
				" FROM job_queue WHERE creator=? ORDER BY id DESC");
			stmt.setString(1, Session::user_id);
		}

		DB::Result res = stmt.executeQuery();
		if (!res.next()) {
			append("There are no jobs to show...");
			return;
		}

		append("<table class=\"jobs\">"
			"<thead>"
				"<tr>"
					"<th class=\"type\">Type</th>"
					"<th class=\"added\">Added<sup>UTC+0</sup></th>"
					"<th class=\"status\">Status</th>",
					(show_all ? "<th class=\"owner\">Owner</th>" : ""),
					"<th class=\"priority\">Priority</th>"
					"<th class=\"info\">Info</th>"
					"<th class=\"actions\">Actions</th>"
				"</tr>"
			"</thead>"
			"<tbody>");

		do {
			JobQueueType job_type {JobQueueType(res.getUInt(3))};
			JobQueueStatus job_status {JobQueueStatus(res.getUInt(4))};
			append("<tr>"
					"<td>", job_type_str(job_type), "</td>"
					"<td><a href=\"/jobs/", res[1], "\" datetime=\"", res[2],
						"\">", res[2], "</a></td>",
					job_status_as_td(job_status));

			if (show_all) // Owner
				append("<td><a href=\"/u/", res[8], "\">", res[9], "</a></td>");

			Permissions job_perms =
				getPermissions(show_all ? res[8] : Session::user_id);

			append("<td>", res[5], "</td>"
					"<td>");

			/* Info + actions */
			auto foo = [&](auto&& append_info, auto&& append_actions) {
				append_info();
				append("</td>"
						"<td>"
							"<a class=\"btn-small\" href=\"/jobs/", res[1],
								"\">View job</a>");
				append_actions();

				if (job_perms & PERM_CANCEL && isIn(job_status,
					{JobQueueStatus::PENDING, JobQueueStatus::IN_PROGRESS}))
				{
					append("<a class=\"btn-small red\" href=\"/jobs/", res[1],
						"/cancel\">Cancel job</a>");
				}

				append("</td>"
					"</tr>");
			};

			// Decide what to show by the job_type
			if (job_type == JobQueueType::JUDGE_SUBMISSION) {
				foo([&]{
					StringView info = res[7];
					string pid = jobs::extractDumpedString(info);
					append("<label>submission</label>",
						"<a href=\"/s/", res[6], "\">", res[6], "</a>"
						"<label>problem</label>",
						"<a href=\"/p/", pid, "\">", pid, "</a>");
				}, [&]{});

			} else if (job_type == JobQueueType::ADD_PROBLEM) {
				foo([&]{
					jobs::AddProblemInfo info {res[7]};
					if (info.name.size())
						append("<label>name</label>", htmlEscape(info.name));
					if (info.memory_limit)
						append("<label>memory limit</label>",
							htmlEscape(toStr(info.memory_limit >> 20)), " MB");
				}, [&]{});

			} else if (job_type == JobQueueType::REUPLOAD_PROBLEM) {
				foo([&]{
					append("TODO...");
				}, [&]{});

			} else if (job_type == JobQueueType::JUDGE_MODEL_SOLUTION) {
				foo([&]{
					append("TODO...");
				}, [&]{});

			} else if (job_type == JobQueueType::EDIT_PROBLEM) {
				foo([&]{
					append("TODO...");
				}, [&]{});

			} else if (job_type == JobQueueType::DELETE_PROBLEM) {
				foo([&]{
					append("TODO...");
				}, [&]{});

			} else
				THROW("Unhandled job_type: ",
					toStr(static_cast<uint>(job_type)));

		} while (res.next());

		append("</tbody>"
			"</table>");

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return error500();
	}
}

void Jobs::job() {
	// TODO: get permissions
	// if (~perms & PERM_VIEW)
	// 	return error403();

	error501();
}

void Jobs::cancelJob() {
	if (~perms & PERM_CANCEL)
		return error403();

	error501();
}

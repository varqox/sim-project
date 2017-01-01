#include "form_validator.h"
#include "jobs.h"

#include <sim/jobs.h>

using std::string;

Jobs::Permissions Jobs::getPermissions(const string& owner_id,
	JobQueueStatus job_status)
{
	if (Session::open()) {
		if (Session::user_type == UTYPE_ADMIN)
			switch (job_status) {
			case JobQueueStatus::PENDING:
			case JobQueueStatus::IN_PROGRESS:
				return Permissions(PERM_VIEW | PERM_VIEW_ALL | PERM_CANCEL);

			case JobQueueStatus::FAILED:
			case JobQueueStatus::CANCELED:
				return Permissions(PERM_VIEW | PERM_VIEW_ALL | PERM_RESTART);

			default:
				return Permissions(PERM_VIEW | PERM_VIEW_ALL);
			}

		if (Session::user_id == owner_id)
			return Permissions(PERM_VIEW | (isIn(job_status,
				{JobQueueStatus::PENDING, JobQueueStatus::IN_PROGRESS}) ?
					(uint)PERM_CANCEL : 0) |
				(Session::user_type == UTYPE_TEACHER ? (int)PERM_VIEW_ALL : 0));
	}

	return PERM_NONE;
}

static constexpr const char* job_type_str(JobQueueType type) {
	switch (type) {
	case JobQueueType::JUDGE_SUBMISSION: return "Judge submission";
	case JobQueueType::ADD_PROBLEM: return "Add problem";
	case JobQueueType::REUPLOAD_PROBLEM: return "Reupload problem";
	case JobQueueType::JUDGE_MODEL_SOLUTION: return "Add problem - judge model "
		"solution";
	case JobQueueType::EDIT_PROBLEM: return "Edit problem";
	case JobQueueType::DELETE_PROBLEM: return "Delete problem";
	}
	return "Unknown";
}

static constexpr const char* job_status_as_td(JobQueueStatus status) {
	switch (status) {
	case JobQueueStatus::VOID: return "<td class=\"status\">Unknown</td>";
	case JobQueueStatus::PENDING: return "<td class=\"status\">Pending</td>";
	case JobQueueStatus::IN_PROGRESS:
		return "<td class=\"status yellow\">In progress</td>";
	case JobQueueStatus::DONE: return "<td class=\"status green\">Done</td>";
	case JobQueueStatus::FAILED: return "<td class=\"status red\">Failed</td>";
	case JobQueueStatus::CANCELED:
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
	// Get permissions to overall job queue
	perms = getPermissions("", JobQueueStatus::VOID);

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
		MySQL::Statement stmt;
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

		MySQL::Result res = stmt.executeQuery();
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

			Permissions job_perms = getPermissions(
				show_all ? res[8] : Session::user_id, job_status);

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

				if (job_perms & PERM_CANCEL)
					append("<a class=\"btn-small red\" onclick=\"cancelJob(",
						res[1], ")\">Cancel job</a>");

				if (job_perms & PERM_RESTART)
					append("<a class=\"btn-small orange\" onclick="
						"\"restartJob(", res[1], ")\">Restart job</a>");

				append("</td>"
					"</tr>");
			};

			// Decide, by the job_type, what to show
			switch (job_type) {
			case JobQueueType::JUDGE_SUBMISSION: {
				foo([&]{
					string info_s = res[7];
					StringView info {info_s};
					string pid = jobs::extractDumpedString(info);
					append("<label>submission</label>",
						"<a href=\"/s/", res[6], "\">", res[6], "</a>"
						"<label>problem</label>",
						"<a href=\"/p/", pid, "\">", pid, "</a>");
				}, [&]{});
				break;
			}

			case JobQueueType::ADD_PROBLEM:
			case JobQueueType::JUDGE_MODEL_SOLUTION: {
				foo([&]{
					jobs::AddProblemInfo info {res[7]};
					if (info.name.size())
						append("<label>name</label>", htmlEscape(info.name));
					if (info.memory_limit)
						append("<label>memory limit</label>",
							toStr(info.memory_limit), " MB");
					append("<label>public</label>",
						(info.public_problem ? "yes" : "no"));

				}, [&]{
					append("<a class=\"btn-small\" href=\"/jobs/", res[1],
						"/report\">Download report</a>");
					if (job_status == JobQueueStatus::DONE)
						append("<a class=\"btn-small green\" href=\"/p/",
							res[6], "\">View problem</a>");
				});
				break;
			}

			case JobQueueType::REUPLOAD_PROBLEM:
			case JobQueueType::EDIT_PROBLEM:
			case JobQueueType::DELETE_PROBLEM: {
				foo([&]{
					append("TODO...");
				}, [&]{});
				break;
			}
			}

		} while (res.next());

		append("</tbody>"
			"</table>");

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return error500();
	}
}

void Jobs::job() {
	// Fetch the job info
	JobQueueType job_type;
	JobQueueStatus job_status;
	string creator, creator_username, added, aux_id, info_str, data_preview;
	try {
		MySQL::Statement stmt = db_conn.prepare(
			"SELECT creator, j.type, status, username, added,"
				" aux_id, info, SUBSTR(data, 1, ?)"
				" FROM job_queue j, users u"
				" WHERE creator=u.id AND j.id=?");
		stmt.setUInt(1, REPORT_PREVIEW_MAX_LENGTH + 1);
		stmt.setString(2, job_id);

		MySQL::Result res = stmt.executeQuery();
		if (!res.next())
			return error404();

		// Get permissions
		creator = res[1];
		job_type = JobQueueType(res.getUInt(2));
		job_status = JobQueueStatus(res.getUInt(3));
		perms = getPermissions(creator, job_status);
		if (~perms & PERM_VIEW)
			return error403();

		// Job's actions
		StringView next_arg = url_args.extractNextArg();
		if (next_arg == "report" && isIn(job_type, {JobQueueType::ADD_PROBLEM,
			JobQueueType::JUDGE_MODEL_SOLUTION,
			JobQueueType::REUPLOAD_PROBLEM}))
		{
			return downloadReport(res[8]);

		} if (next_arg == "cancel")
			return cancelJob();
		if (next_arg == "restart")
			return restartJob(job_type, res[7]);
		if (next_arg != "")
			return error404();

		// Get other fields
		creator_username = res[4];
		added = res[5];
		aux_id = (res.isNull(6) ? "" : res[6]);
		info_str = res[7];
		data_preview = res[8];

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return error500();
	}

	jobsTemplate("Job " + job_id);

	auto foo = [&](auto&& show_actions, auto&& append_info) {
		append("<div class=\"job-info\">"
			"<div>"
				"<h1>Job ", job_id, "</h1>"
				"<div>");

		show_actions();

		if (perms & PERM_CANCEL)
			append("<a class=\"btn-small red\" onclick=\"cancelJob(", job_id,
				")\">Cancel job</a>");

		if (perms & PERM_RESTART)
			append("<a class=\"btn-small orange\" onclick=\"restartJob(",
				job_id, ")\">Restart job</a>");

		append("</div>"
			"</div>"
			"<table>"
				"<thead>"
					"<tr>"
						"<th style=\"min-width:120px\">Type</th>"
						"<th style=\"min-width:150px\">Added</th>"
						"<th style=\"min-width:150px\">Status</th>");

		bool show_owner = (Session::user_id != creator);
		if (show_owner)
			append("<th style=\"min-width:120px\">Owner</th>");

		append("<th style=\"min-width:90px\">Info</th>"
					"</tr>"
				"</thead>"
				"<tbody>"
					"<tr>"
						"<td>", job_type_str(job_type), "</td>"
						"<td datetime=\"", added, "\">", added,
							"<sup>UTC+0</sup></td>",
						job_status_as_td(job_status));

		if (show_owner)
			append("<td><a href=\"/u/", creator, "\">", creator_username,
				"</a></td>");

		append("<td style=\"text-align: left\">");
		append_info();
		append("</td>"
					"</tr>"
				"</tbody>"
			"</table>"
			"</div>");
	};

	// Decide, by the job_type, what to show
	switch (job_type) {
	case JobQueueType::JUDGE_SUBMISSION: {
		foo([&]{}, [&]{
			StringView info {info_str};
			string pid = jobs::extractDumpedString(info);
			append("<label>submission</label>",
				"<a href=\"/s/", aux_id, "\">", aux_id, "</a><br/>"
				"<label>problem</label>",
				"<a href=\"/p/", pid, "\">", pid, "</a><br/>");
		});
		break;
	}

	case JobQueueType::ADD_PROBLEM:
	case JobQueueType::JUDGE_MODEL_SOLUTION: {
		foo([&]{
			append("<a class=\"btn-small\" href=\"/jobs/", job_id,
				"/report\">Download report</a>");
			if (job_status == JobQueueStatus::DONE)
				append("<a class=\"btn-small green\" href=\"/p/", aux_id,
					"\">View problem</a>");
		}, [&]{
			jobs::AddProblemInfo info {info_str};
			if (info.name.size())
				append("<label>name</label>", htmlEscape(info.name), "<br/>");
			if (info.label.size())
				append("<label>label</label>", htmlEscape(info.label), "<br/>");
			if (info.memory_limit)
				append("<label>memory limit</label>", toStr(info.memory_limit),
					" MB", "<br/>");
			if (info.global_time_limit)
				append("<label>global time limit</label>",
					usecToSecStr(info.global_time_limit, 6), " s", "<br/>");
			append("<label>auto time limit setting</label>",
				(info.force_auto_limit ? "yes" : "no"), "<br/>");
			append("<label>ignore simfile</label>",
				(info.ignore_simfile ? "yes" : "no"), "<br/>");
			append("<label>public problem</label>",
				(info.public_problem ? "yes" : "no"), "<br/>");
		});

		bool is_incomplete = (data_preview.size() > REPORT_PREVIEW_MAX_LENGTH);
		if (is_incomplete)
			data_preview.resize(REPORT_PREVIEW_MAX_LENGTH);

		append("<h2>Report preview</h2>"
			"<pre class=\"report-preview\">", htmlEscape(data_preview),
				"</pre>");

		if (is_incomplete)
			append("<p>The report is too large to show it entirely here. If you"
				" want to see the whole, click: "
				"<a class=\"btn-small\" href=\"/jobs/", job_id, "/report\">"
					"Download the full report</a></p>");
		break;
	}

	case JobQueueType::REUPLOAD_PROBLEM:
	case JobQueueType::EDIT_PROBLEM:
	case JobQueueType::DELETE_PROBLEM: {
		foo([&]{}, [&]{
			append("TODO...");
		});
		break;
	}
	}
}

void Jobs::downloadReport(std::string data_preview) {
	// Assumption: permissions are already checked
	try {
		resp.headers["Content-type"] = "application/text";
		resp.headers["Content-Disposition"] =
			concat("attachment; filename=job-", job_id, "-report");

		// The whole report is already fetched
		if (data_preview.size() <= REPORT_PREVIEW_MAX_LENGTH) {
			resp.content = std::move(data_preview);
			return;
		}

		// We have to fetch the whole report
		MySQL::Result res = db_conn.executeQuery("SELECT data FROM job_queue"
			" WHERE id=" + job_id);
		if (!res.next())
			return error404();

		resp.content = res[1];

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return error500();
	}
}

void Jobs::cancelJob() {
	if (~perms & PERM_CANCEL) {
		if (perms & PERM_VIEW)
			return response("400 Bad Request",
				"Job has already been canceled or done");
		return response("403 Forbidden");
	}

	if (req->method != server::HttpRequest::POST)
		return response("404 Not Found");

	FormValidator fv(req->form_data);
	if (fv.get("csrf_token") != Session::csrf_token)
		return response("403 Forbidden");

	// Cancel job
	try {
		db_conn.executeUpdate("UPDATE job_queue"
			" SET status=" JQSTATUS_CANCELED_STR " WHERE id=" + job_id);

		response("200 OK");

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return response("500 Internal Server Error");
	}
}

void Jobs::restartJob(JobQueueType job_type, StringView job_info) {
	if (~perms & PERM_RESTART) {
		if (perms & PERM_VIEW)
			return response("400 Bad Request",
				"Job has already been restarted");
		return response("403 Forbidden");
	}

	if (req->method != server::HttpRequest::POST)
		return response("404 Not Found");

	FormValidator fv(req->form_data);
	if (fv.get("csrf_token") != Session::csrf_token)
		return response("403 Forbidden");

	// Restart job
	try {
		// Restart adding problem
		if (isIn(job_type,
			{JobQueueType::ADD_PROBLEM, JobQueueType::JUDGE_MODEL_SOLUTION}))
		{
			jobs::AddProblemInfo info {job_info};
			info.stage = jobs::AddProblemInfo::FIRST;
			MySQL::Statement stmt = db_conn.prepare("UPDATE job_queue"
				" SET type=" JQTYPE_ADD_PROBLEM_STR ","
					" status=" JQSTATUS_PENDING_STR ", info=? WHERE id=?");
			stmt.setString(1, info.dump());
			stmt.setString(2, job_id);
			stmt.executeUpdate();

		} else
			db_conn.executeUpdate("UPDATE job_queue"
				" SET status=" JQSTATUS_PENDING_STR " WHERE id=" + job_id);

		notifyJobServer();
		response("200 OK");

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return response("500 Internal Server Error");
	}
}

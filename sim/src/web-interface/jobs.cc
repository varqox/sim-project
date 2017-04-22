#include "sim.h"

#include <sim/jobs.h>

using std::string;

Sim::JobPermissions Sim::jobs_get_permissions(StringView owner_id,
	JobQueueStatus job_status)
{
	using Sim::JobPermissions::PERM_NONE;
	using Sim::JobPermissions::PERM_VIEW;
	using Sim::JobPermissions::PERM_VIEW_ALL;
	using Sim::JobPermissions::PERM_CANCEL;
	using Sim::JobPermissions::PERM_RESTART;

	D(throw_assert(session_is_open);) // Session must be open to access jobs

	if (session_user_type == UTYPE_ADMIN)
		switch (job_status) {
		case JobQueueStatus::PENDING:
		case JobQueueStatus::IN_PROGRESS:
			return PERM_VIEW | PERM_VIEW_ALL | PERM_CANCEL;

		case JobQueueStatus::FAILED:
		case JobQueueStatus::CANCELED:
			return PERM_VIEW | PERM_VIEW_ALL | PERM_RESTART;

		default:
			return PERM_VIEW | PERM_VIEW_ALL;
		}

	static_assert((uint)PERM_NONE == 0, "Needed blow");
	if (session_user_id == owner_id)
		return PERM_VIEW | (isIn(job_status,
			{JobQueueStatus::PENDING, JobQueueStatus::IN_PROGRESS}) ?
				PERM_CANCEL : PERM_NONE) |
			(session_user_type == UTYPE_TEACHER ? PERM_VIEW_ALL : PERM_NONE);

	return PERM_NONE;
}

static constexpr const char* job_type_str(JobQueueType type) noexcept {
	switch (type) {
	case JobQueueType::JUDGE_SUBMISSION: return "Judge submission";
	case JobQueueType::ADD_PROBLEM: return "Add problem";
	case JobQueueType::REUPLOAD_PROBLEM: return "Reupload problem";
	case JobQueueType::ADD_JUDGE_MODEL_SOLUTION:
		return "Add problem - set limits";
	case JobQueueType::REUPLOAD_JUDGE_MODEL_SOLUTION:
		return"Reupload problem - set limits";
	case JobQueueType::EDIT_PROBLEM: return "Edit problem";
	case JobQueueType::DELETE_PROBLEM: return "Delete problem";
	case JobQueueType::VOID: return "Void";
	}
	return "Unknown";
}

static constexpr const char* job_status_as_td(JobQueueStatus status) noexcept  {
	switch (status) {
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

void Sim::jobs_handle() {
	using Sim::JobPermissions::PERM_VIEW_ALL;
	using Sim::JobPermissions::PERM_CANCEL;
	using Sim::JobPermissions::PERM_RESTART;

	if (not session_open())
		return redirect("/login?" + request.target);

	StringView next_arg = url_args.extractNextArg();
	if (isDigit(next_arg)) {
		jobs_job_id = next_arg;
		return error501();
		// return job();
	}

	// Get permissions to overall job queue
	jobs_perms = jobs_get_permissions("", JobQueueStatus::DONE);

	if (next_arg == "cancel")
		return error501();
		// return cancelJob();

	else if (!next_arg.empty() && next_arg != "my")
		return error404();

	bool show_all = (next_arg != "my");
	if (show_all && uint(~jobs_perms & PERM_VIEW_ALL))
		return error403();

	jobs_page_template("Job queue");
	append(next_arg.empty() ? "<h1>Job queue</h1>" : "<h1>My jobs</h1>");

	// List jobs
	try {
		MySQL::Statement<> stmt;
		uint8_t jtype, jstatus;
		my_bool is_aux_id_null, is_creator_null;
		InplaceBuff<30> job_id, added, jpriority, aux_id, creator;
		InplaceBuff<512> jinfo;
		InplaceBuff<USERNAME_MAX_LEN> cusername;

		if (show_all) {
			stmt = mysql.prepare("SELECT j.id, added, j.type, status,"
					" priority, aux_id, info, creator, username"
				" FROM job_queue j LEFT JOIN users u ON creator=u.id"
				" WHERE j.type!=" JQTYPE_VOID_STR " ORDER BY id DESC");
			stmt.execute();
			stmt.res_bind(7, creator);
			stmt.res_bind_isnull(7, is_creator_null);
			stmt.res_bind(8, cusername);

		} else {
			stmt = mysql.prepare("SELECT id, added, type, status, priority,"
				" aux_id, info"
				" FROM job_queue WHERE creator=? AND type!=" JQTYPE_VOID_STR
				" ORDER BY id DESC");
			stmt.bindAndExecute(session_user_id);
		}

		stmt.res_bind(0, job_id);
		stmt.res_bind(1, added);
		stmt.res_bind(2, jtype);
		stmt.res_bind(3, jstatus);
		stmt.res_bind(4, jpriority);
		stmt.res_bind(5, aux_id);
		stmt.res_bind_isnull(5, is_aux_id_null);
		stmt.res_bind(6, jinfo);
		stmt.resFixBinds();

		if (!stmt.next()) {
			append("There are no jobs to show...");
			return;
		}

		append("<table class=\"jobs\">"
			"<thead>"
				"<tr>"
					"<th class=\"type\">Type</th>"
					"<th class=\"added\">Added<sup>UTC</sup></th>"
					"<th class=\"status\">Status</th>",
					(show_all ? "<th class=\"owner\">Owner</th>" : ""),
					"<th class=\"priority\">Priority</th>"
					"<th class=\"info\">Info</th>"
					"<th class=\"actions\">Actions</th>"
				"</tr>"
			"</thead>"
			"<tbody>");

		do {
			JobQueueType job_type {JobQueueType(jtype)};
			JobQueueStatus job_status {JobQueueStatus(jstatus)};
			append("<tr>"
					"<td>", job_type_str(job_type), "</td>"
					"<td><a href=\"/jobs/", job_id, "\" datetime=\"", added,
						"\">", added, "</a></td>",
					job_status_as_td(job_status));

			// Owner
			if (show_all) {
				if (is_creator_null)
					append("<td>System</td>");
				else
					append("<td><a href=\"/u/", creator, "\">", cusername,
						"</a></td>");
			}

			JobPermissions job_perms = jobs_get_permissions(show_all ?
				creator : session_user_id, job_status);

			append("<td>", jpriority, "</td>"
					"<td>");

			/* Info + actions */
			auto foo = [&](auto&& append_info, auto&& append_actions) {
				append_info();
				append("</td>"
						"<td>"
							"<a class=\"btn-small\" href=\"/jobs/", job_id,
								"\">View job</a>");
				append_actions();

				if (uint(job_perms & PERM_CANCEL))
					append("<a class=\"btn-small red\" onclick=\"cancelJob(",
						job_id, ")\">Cancel job</a>");

				if (uint(job_perms & PERM_RESTART))
					append("<a class=\"btn-small orange\" onclick="
						"\"restartJob(", job_id, ")\">Restart job</a>");

				append("</td>"
					"</tr>");
			};

			// Decide, by the job_type, what to show
			switch (job_type) {
			case JobQueueType::JUDGE_SUBMISSION: {
				foo([&]{
					StringView info {jinfo};
					string pid = jobs::extractDumpedString(info);
					append("<label>submission</label>",
						"<a href=\"/s/", aux_id, "\">", aux_id, "</a>"
						"<label>problem</label>",
						"<a href=\"/p/", pid, "\">", pid, "</a>");
				}, [&]{});
				break;
			}

			case JobQueueType::ADD_PROBLEM:
			case JobQueueType::REUPLOAD_PROBLEM:
			case JobQueueType::ADD_JUDGE_MODEL_SOLUTION:
			case JobQueueType::REUPLOAD_JUDGE_MODEL_SOLUTION: {
				foo([&]{
					jobs::AddProblemInfo info {jinfo};
					if (info.name.size())
						append("<label>name</label>", htmlEscape(info.name));
					if (info.memory_limit)
						append("<label>memory limit</label>", info.memory_limit,
							" MB");
					append("<label>public</label>",
						(info.public_problem ? "yes" : "no"));

				}, [&]{
					append("<div class=\"dropmenu down\">"
							"<a class=\"btn-small dropmenu-toggle\">"
								"Download</a>"
							"<ul>"
								"<a href=\"/jobs/", job_id, "/report\">"
									"Report</a>"
								"<a href=\"/jobs/", job_id,
									"/download-uploaded-package\">"
									"Uploaded package</a>"
							"</ul>"
						"</div>");
					if (job_status == JobQueueStatus::DONE || isIn(job_type, {
						JobQueueType::REUPLOAD_PROBLEM,
						JobQueueType::REUPLOAD_JUDGE_MODEL_SOLUTION}))
					{
						append("<a class=\"btn-small green\" href=\"/p/",
							aux_id, "\">View problem</a>");
					}
				});
				break;
			}

			case JobQueueType::EDIT_PROBLEM:
			case JobQueueType::DELETE_PROBLEM: {
				foo([&]{
					append("TODO...");
				}, [&]{});
				break;
			}

			case JobQueueType::VOID:
				break;
			}

		} while (stmt.next());

		append("</tbody>"
			"</table>");

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return error500();
	}
}

#if 0
void Jobs::job() {
	// Fetch the job info
	JobQueueType job_type;
	JobQueueStatus job_status;
	string creator, creator_username, added, aux_id, info_str, data_preview;
	try {
		MySQL::Statement stmt = db_conn.prepare(
			"SELECT creator, j.type, status, username, added,"
				" aux_id, info, SUBSTR(data, 1, ?)"
				" FROM job_queue j LEFT JOIN users u ON j.creator=u.id"
				" WHERE j.id=? AND j.type!=" JQTYPE_VOID_STR);
		stmt.setUInt(1, REPORT_PREVIEW_MAX_LENGTH + 1);
		stmt.setString(2, job_id);

		MySQL::Result res = stmt.executeQuery();
		if (!res.next())
			return error404();

		// Get permissions
		creator = (res.isNull(1) ? "" : res[1]);
		job_type = JobQueueType(res.getUInt(2));
		job_status = JobQueueStatus(res.getUInt(3));
		perms = getPermissions(creator, job_status);
		if (~perms & PERM_VIEW)
			return error403();

		// Job's actions
		StringView next_arg = url_args.extractNextArg();
		if (isIn(job_type, {JobQueueType::ADD_PROBLEM,
			JobQueueType::REUPLOAD_PROBLEM,
			JobQueueType::ADD_JUDGE_MODEL_SOLUTION,
			JobQueueType::REUPLOAD_JUDGE_MODEL_SOLUTION}))
		{
			if (next_arg == "report")
				return downloadReport(res[8]);

			if (next_arg == "download-uploaded-package")
				return downloadUploadedPackage();

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
							"<sup>UTC</sup></td>",
						job_status_as_td(job_status));

		if (show_owner) {
			if (creator.empty())
				append("<td>System</td>");
			else
				append("<td><a href=\"/u/", creator, "\">", creator_username,
					"</a></td>");
		}

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
	case JobQueueType::REUPLOAD_PROBLEM:
	case JobQueueType::ADD_JUDGE_MODEL_SOLUTION:
	case JobQueueType::REUPLOAD_JUDGE_MODEL_SOLUTION: {
		foo([&]{
			append("<div class=\"dropmenu down\">"
					"<a class=\"btn-small dropmenu-toggle\">Download</a>"
					"<ul>"
						"<a href=\"/jobs/", job_id, "/report\">Report</a>"
						"<a href=\"/jobs/", job_id,
							"/download-uploaded-package\">Uploaded package</a>"
					"</ul>"
				"</div>");
			if (job_status == JobQueueStatus::DONE || isIn(job_type, {
				JobQueueType::REUPLOAD_PROBLEM,
				JobQueueType::REUPLOAD_JUDGE_MODEL_SOLUTION}))
			{
				append("<a class=\"btn-small green\" href=\"/p/", aux_id,
					"\">View problem</a>");
			}
		}, [&]{
			jobs::AddProblemInfo info {info_str};
			if (info.name.size())
				append("<label>name</label>", htmlEscape(info.name), "<br/>");
			if (info.label.size())
				append("<label>label</label>", htmlEscape(info.label), "<br/>");
			if (info.memory_limit)
				append("<label>memory limit</label>", info.memory_limit, " MB",
					"<br/>");
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

	case JobQueueType::EDIT_PROBLEM:
	case JobQueueType::DELETE_PROBLEM: {
		foo([&]{}, [&]{
			append("TODO...");
		});
		break;
	}

	case JobQueueType::VOID:
		break;
	}
}

void Jobs::downloadReport(string data_preview) {
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

void Jobs::downloadUploadedPackage() {
	resp.headers["Content-Disposition"] =
		concat("attachment; filename=", job_id, ".zip");
	resp.content_type = server::HttpResponse::FILE;
	resp.content = concat("jobs_files/", job_id, ".zip");
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
		bool add = isIn(job_type, {JobQueueType::ADD_PROBLEM,
			JobQueueType::ADD_JUDGE_MODEL_SOLUTION});
		bool reupload = isIn(job_type, {JobQueueType::REUPLOAD_PROBLEM,
			JobQueueType::REUPLOAD_JUDGE_MODEL_SOLUTION});
		if (add || reupload) {
			jobs::AddProblemInfo info {job_info};
			info.stage = jobs::AddProblemInfo::FIRST;
			MySQL::Statement stmt = db_conn.prepare("UPDATE job_queue"
				" SET type=?, status=" JQSTATUS_PENDING_STR ", info=?"
				" WHERE id=?");
			stmt.setUInt(1, uint(add ? JobQueueType::ADD_PROBLEM
				: JobQueueType::REUPLOAD_PROBLEM));
			stmt.setString(2, info.dump());
			stmt.setString(3, job_id);
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
#endif
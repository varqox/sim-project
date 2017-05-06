#include "sim.h"

#include <sim/jobs.h>
#include <simlib/filesystem.h>

using std::array;

void Sim::api_handle() {
	STACK_UNWINDING_MARK;

	StringView next_arg = url_args.extractNextArg();
	if (next_arg == "logs")
		return api_logs();
	else if (next_arg == "jobs")
		return api_jobs();
	else
		return set_response("404 Not Found");
}

void Sim::api_logs() {
	STACK_UNWINDING_MARK;

	if (!session_open() || session_user_type > UTYPE_ADMIN)
		return set_response("403 Forbidden");

	StringView type = url_args.extractNextArg();
	CStringView filename;
	if (type == "web")
		filename = SERVER_LOG;
	else if (type == "web_err")
		filename = SERVER_ERROR_LOG;
	else if (type == "jobs")
		filename = JOB_SERVER_LOG;
	else if (type == "jobs_err")
		filename = JOB_SERVER_ERROR_LOG;
	else
		return set_response("404 Not Found");

	off64_t end_offset = 0;
	StringView que = url_args.extractQuery();
	if (que.size()) {
		if (strtou(que, end_offset) == -1)
			return set_response("400 Bad Request");

		// If overflow occurred
		if (end_offset < 0)
			end_offset = 0;
	}

	FileDescriptor fd {filename, O_RDONLY | O_LARGEFILE};
	off64_t fsize = lseek64(fd, 0, SEEK_END);
	throw_assert(fsize >= 0);
	if (que.empty())
		end_offset = fsize;
	else if (end_offset > fsize)
		end_offset = fsize;

	constexpr uint CHUNK_MAX_LEN = 16384;
	uint len;

	if (end_offset < CHUNK_MAX_LEN) {
		len = end_offset;
		if (lseek64(fd, 0, SEEK_SET) == -1)
			THROW("lseek64()", error(errno));

	} else {
		len = CHUNK_MAX_LEN;
		if (lseek64(fd, end_offset - CHUNK_MAX_LEN, SEEK_SET) == -1)
			THROW("lseek64()", error(errno));
	}

	// Read the data
	InplaceBuff<CHUNK_MAX_LEN> buff(len);
	auto ret = readAll(fd, buff.data(), len);
	if (ret != len)
		THROW("read()", error(errno));

	append(end_offset - len, '\n'); // New offset
	append(toHex(buff)); // Data
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

void Sim::api_jobs() {
	STACK_UNWINDING_MARK;

	if (!session_open())
		return set_response("403 Forbidden");

	constexpr auto PERM_VIEW_ALL = Sim::JobPermissions::PERM_VIEW_ALL;
	constexpr auto PERM_CANCEL = Sim::JobPermissions::PERM_CANCEL;
	constexpr auto PERM_RESTART = Sim::JobPermissions::PERM_RESTART;

	// Get permissions to the overall job queue
	jobs_perms = jobs_get_permissions("", JobQueueStatus::DONE);
	StringView next_arg = url_args.extractNextArg();
	bool my_jobs = false, select_data = false;

	InplaceArray<InplaceBuff<30>, 5> to_bind;
	InplaceBuff<512> qfields, qwhere;
	qfields.append("SELECT j.id, added, j.type, j.status, j.priority, j.aux_id,"
			" j.info, j.creator, u.username");
	qwhere.append(" FROM job_queue j LEFT JOIN users u ON creator=u.id"
		" WHERE j.type!=" JQTYPE_VOID_STR);


	// Request for user's jobs
	if (next_arg == "my") {
		next_arg = url_args.extractNextArg();
		qwhere.append(" AND creator=?");
		to_bind.emplace_back(session_user_id);
		my_jobs = true;
	// Request for all jobs
	} else {
		if (not uint(jobs_perms & PERM_VIEW_ALL))
			return set_response("403 Forbidden");
	}

	MySQL::Statement<> stmt;

	// Process restrictions
	for (uint i = 0, mask = 0;
		i < 2 and next_arg.size();
		++i, next_arg = url_args.extractNextArg())
	{
		constexpr uint ID_COND = 1;
		constexpr uint AUX_ID_COND = 2;

		auto arg = decodeURI(next_arg);
		char cond = arg[0];
		StringView arg_id = StringView{arg}.substr(1);

		if (not isDigit(arg_id))
			return set_response("400 Bad Request");

		// conditional
		if (cond == '<' and ~mask & ID_COND) {
			qwhere.append(" AND j.id<?");
			to_bind.emplace_back(arg_id);
			mask |= ID_COND;

		} else if (cond == '=' and ~mask & ID_COND) {
			qfields.append(", SUBSTR(data, 1, ",
				meta::ToString<REPORT_PREVIEW_MAX_LENGTH + 1>{}, ')');
			select_data = true;
			qwhere.append(" AND j.id=?");
			to_bind.emplace_back(arg_id);
			mask |= ID_COND;

		// problem
		} else if (cond == 'p' and ~mask & AUX_ID_COND) {
			qwhere.append(" AND j.aux_id=? AND j.type IN("
				JQTYPE_ADD_PROBLEM_STR ","
				JQTYPE_ADD_JUDGE_MODEL_SOLUTION_STR ","
				JQTYPE_REUPLOAD_PROBLEM_STR ","
				JQTYPE_REUPLOAD_JUDGE_MODEL_SOLUTION_STR ","
				JQTYPE_DELETE_PROBLEM_STR ","
				JQTYPE_EDIT_PROBLEM_STR ")");

			to_bind.emplace_back(arg_id);
			mask |= AUX_ID_COND;

		// submission
		} else if (cond == 's' and ~mask & AUX_ID_COND) {
			qwhere.append(" AND j.aux_id=? AND j.type="
				JQTYPE_JUDGE_SUBMISSION_STR);

			to_bind.emplace_back(arg_id);
			mask |= AUX_ID_COND;

		} else
			return set_response("400 Bad Request");

	}

	// Execute query
	stmt =
		mysql.prepare(concat(qfields, qwhere, " ORDER BY j.id DESC LIMIT 50"));

	for (uint i = 0; i < to_bind.size(); ++i)
		stmt.bind(i, to_bind[i]);
	stmt.fixBinds();
	stmt.execute();

	resp.headers["content-type"] = "text/plain; charset=utf-8";
	append("[");

	uint8_t jtype, jstatus;
	my_bool is_aux_id_null, is_creator_null;
	InplaceBuff<30> job_id, added, jpriority, aux_id, creator;
	InplaceBuff<512> jinfo;
	InplaceBuff<USERNAME_MAX_LEN> cusername;
	InplaceBuff<REPORT_PREVIEW_MAX_LENGTH> report_preview;

	stmt.res_bind(0, job_id);
	stmt.res_bind(1, added);
	stmt.res_bind(2, jtype);
	stmt.res_bind(3, jstatus);
	stmt.res_bind(4, jpriority);
	stmt.res_bind(5, aux_id);
	stmt.res_bind_isnull(5, is_aux_id_null);
	stmt.res_bind(6, jinfo);
	stmt.res_bind(7, creator);
	stmt.res_bind_isnull(7, is_creator_null);
	stmt.res_bind(8, cusername);
	if (select_data)
		stmt.res_bind(9, report_preview);
	stmt.resFixBinds();

	while (stmt.next()) {
		JobQueueType job_type {JobQueueType(jtype)};
		JobQueueStatus job_status {JobQueueStatus(jstatus)};

		append("\n[", job_id, ',',
			jsonStringify(added), ","
			"\"", job_type_str(job_type), "\",");

		// Status: (CSS class, text)
		switch (job_status) {
		case JobQueueStatus::PENDING: append("[\"\",\"Pending\"],"); break;
		case JobQueueStatus::IN_PROGRESS:
			append("[\"yellow\",\"In progress\"],"); break;
		case JobQueueStatus::DONE: append("[\"green\",\"Done\"],"); break;
		case JobQueueStatus::FAILED: append("[\"red\",\"Failed\"],"); break;
		case JobQueueStatus::CANCELED:
			append("[\"blue\",\"Cancelled\"],"); break;
		}

		append(jpriority, ',');

		// Creator
		if (is_creator_null)
			append("null,null,");
		else
			append(creator, ",\"", cusername, "\",");

		// Additional info
		InplaceBuff<10> actions;
		append('{');

		switch (job_type) {
		case JobQueueType::JUDGE_SUBMISSION: {
			StringView x {jinfo};
			append("\"problem\":", jobs::extractDumpedString(x));
			append(",\"submission\":", aux_id);
			break;
		}

		case JobQueueType::ADD_PROBLEM:
		case JobQueueType::REUPLOAD_PROBLEM:
		case JobQueueType::ADD_JUDGE_MODEL_SOLUTION:
		case JobQueueType::REUPLOAD_JUDGE_MODEL_SOLUTION: {
			jobs::AddProblemInfo info {jinfo};
			append("\"make public\":", info.make_public ?
				"\"yes\"" : "\"no\"");

			if (not is_aux_id_null)
				append(",\"problem\":", aux_id);
			if (info.name.size())
				append(",\"name\":", jsonStringify(info.name));
			if (info.label.size())
				append(",\"label\":", jsonStringify(info.label));
			if (info.memory_limit)
				append(",\"memory limit\":\"", info.memory_limit, " MB\"");
			if (info.global_time_limit)
				append(",\"global time limit\":",
					usecToSecStr(info.global_time_limit, 6));

			append(",\"auto time limit setting\":", info.force_auto_limit ?
				"\"yes\"" : "\"no\"");
			append(",\"ignore simfile\":", info.ignore_simfile ?
				"\"yes\"" : "\"no\"");

			// Add actions
			actions.append('P'); // dropdown with report and uploaded package

			if (not is_aux_id_null)
				actions.append('V'); // View problem
			break;
		}

		default:
			break;
		}
		append("},");


		// Append what buttons to show
		append('"', actions);

		auto job_perms = jobs_get_permissions(my_jobs ? session_user_id
			: creator, job_status);
		if (uint(job_perms & PERM_CANCEL))
			append('c');
		if (uint(job_perms & PERM_RESTART))
			append('r');
		append('\"');

		// Append report preview (whether there is more to load, data)
		if (select_data)
			append(",[", report_preview.size > REPORT_PREVIEW_MAX_LENGTH, ',',
				jsonStringify(report_preview), ']');

		append("],");
	}

	if (resp.content.back() == ',')
		--resp.content.size;

	append("\n]");
}

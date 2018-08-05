#include "sim.h"

#include <simlib/filesystem.h>

void Sim::api_handle() {
	STACK_UNWINDING_MARK;

	resp.headers["Content-type"] = "text/plain; charset=utf-8";

	StringView next_arg = url_args.extractNextArg();
	if (next_arg == "logs")
		return api_logs();
	else if (next_arg == "contests")
		return api_contests();
	else if (next_arg == "contest")
		return api_contest();
	else if (next_arg == "contest_user")
		return api_contest_user();
	else if (next_arg == "contest_users")
		return api_contest_users();
	else if (next_arg == "job")
		return api_job();
	else if (next_arg == "jobs")
		return api_jobs();
	else if (next_arg == "problem")
		return api_problem();
	else if (next_arg == "problems")
		return api_problems();
	else if (next_arg == "submission")
		return api_submission();
	else if (next_arg == "submissions")
		return api_submissions();
	else if (next_arg == "user")
		return api_user();
	else if (next_arg == "users")
		return api_users();
	else
		return api_error404();
}

void Sim::api_logs() {
	STACK_UNWINDING_MARK;

	if (not session_is_open || session_user_type != UserType::ADMIN)
		return api_error403();

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
		return api_error404();

	off64_t end_offset = 0;
	StringView que = url_args.extractQuery();
	if (que.size()) {
		if (strtou(que, end_offset) == -1)
			return api_error400();

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

	constexpr uint CHUNK_MAX_LEN = 4 << 10; // 4 kB
	uint len;

	if (end_offset < CHUNK_MAX_LEN) {
		len = end_offset;
		if (lseek64(fd, 0, SEEK_SET) == -1)
			THROW("lseek64()", errmsg());

	} else {
		len = CHUNK_MAX_LEN;
		if (lseek64(fd, end_offset - CHUNK_MAX_LEN, SEEK_SET) == -1)
			THROW("lseek64()", errmsg());
	}

	// Read the data
	InplaceBuff<CHUNK_MAX_LEN> buff(len);
	auto ret = readAll(fd, buff.data(), len);
	// TODO: readAll() and writeAll() - support offset argument - p(write|read)
	// TODO: getFileContents() - support offset argument - pread()
	// TODO: putFileContents() - support offset argument - pwrite()
	if (ret != len)
		THROW("read()", errmsg());

	append(end_offset - len, '\n'); // New offset
	append(toHex(buff)); // Data
}

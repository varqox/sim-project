#include "sim.hh"

#include <simlib/file_contents.hh>
#include <simlib/file_descriptor.hh>

using sim::User;

void Sim::api_handle() {
	STACK_UNWINDING_MARK;

	resp.headers["Content-type"] = "text/plain; charset=utf-8";
	// Allow download queries to pass without POST
	StringView next_arg = url_args.extract_next_arg();
	if (next_arg == "download") {
		next_arg = url_args.extract_next_arg();
		if (is_one_of(next_arg, "submission", "problem", "contest_file")) {
			auto id = url_args.extract_next_arg();
			request.target =
			   concat_tostr("/api/", next_arg, '/', id, "/download");

		} else if (next_arg == "statement") {
			next_arg = url_args.extract_next_arg();
			if (is_one_of(next_arg, "problem", "contest")) {
				auto id = url_args.extract_next_arg();
				request.target =
				   concat_tostr("/api/", next_arg, '/', id, "/statement");
			} else {
				return api_error404();
			}

		} else if (next_arg == "job") {
			auto job_id = url_args.extract_next_arg();
			next_arg = url_args.extract_next_arg();
			if (is_one_of(next_arg, "log", "uploaded-package",
			              "uploaded-statement")) {
				request.target =
				   concat_tostr("/api/job/", job_id, '/', next_arg);
			} else {
				return api_error404();
			}

		} else {
			return api_error404();
		}

		// Update url_args to reflect the changed URL
		url_args = RequestUriParser(request.target);
		url_args.extract_next_arg(); // extract "/api"
		next_arg = url_args.extract_next_arg();

	} else if (request.method != server::HttpRequest::POST) {
		return api_error403("To access API you have to use POST");
	}

	if (next_arg == "contest") {
		return api_contest();
	}
	if (next_arg == "contest_entry_token") {
		return api_contest_entry_token();
	}
	if (next_arg == "contest_file") {
		return api_contest_file();
	}
	if (next_arg == "contest_files") {
		return api_contest_files();
	}
	if (next_arg == "contest_user") {
		return api_contest_user();
	}
	if (next_arg == "contest_users") {
		return api_contest_users();
	}
	if (next_arg == "contests") {
		return api_contests();
	}
	if (next_arg == "job") {
		return api_job();
	}
	if (next_arg == "jobs") {
		return api_jobs();
	}
	if (next_arg == "logs") {
		return api_logs();
	}
	if (next_arg == "problem") {
		return api_problem();
	}
	if (next_arg == "problems") {
		return api_problems();
	}
	if (next_arg == "submission") {
		return api_submission();
	}
	if (next_arg == "submissions") {
		return api_submissions();
	}
	if (next_arg == "user") {
		return api_user();
	}
	if (next_arg == "users") {
		return api_users();
	}
	return api_error404();
}

void Sim::api_logs() {
	STACK_UNWINDING_MARK;

	if (not session_is_open || session_user_type != User::Type::ADMIN) {
		return api_error403();
	}

	StringView type = url_args.extract_next_arg();
	CStringView filename;
	if (type == "web") {
		filename = SERVER_LOG;
	} else if (type == "web_err") {
		filename = SERVER_ERROR_LOG;
	} else if (type == "jobs") {
		filename = JOB_SERVER_LOG;
	} else if (type == "jobs_err") {
		filename = JOB_SERVER_ERROR_LOG;
	} else {
		return api_error404();
	}

	off64_t end_offset = 0;
	StringView query = url_args.extract_query();
	uint chunk_max_len = LOGS_FIRST_CHUNK_MAX_LEN;
	if (!query.empty()) {
		auto opt = str2num<off64_t>(query);
		if (not opt or *opt < 0) {
			return api_error400();
		}

		end_offset = *opt;
		chunk_max_len = LOGS_OTHER_CHUNK_MAX_LEN;
	}

	FileDescriptor fd(filename, O_RDONLY | O_CLOEXEC);
	off64_t fsize = lseek64(fd, 0, SEEK_END);
	throw_assert(fsize >= 0);
	if (query.empty() or end_offset > fsize) {
		end_offset = fsize;
	}

	size_t len = 0;
	if (end_offset < chunk_max_len) {
		len = end_offset;
		if (lseek64(fd, 0, SEEK_SET) == -1) {
			THROW("lseek64()", errmsg());
		}

	} else {
		len = chunk_max_len;
		if (lseek64(fd, end_offset - chunk_max_len, SEEK_SET) == -1) {
			THROW("lseek64()", errmsg());
		}
	}

	// Read the data
	InplaceBuff<meta::max(LOGS_FIRST_CHUNK_MAX_LEN, LOGS_OTHER_CHUNK_MAX_LEN)>
	   buff(len);
	auto ret = read_all(fd, buff.data(), len);
	// TODO: read_all() and write_all() - support offset argument -
	// p(write|read)
	if (ret != len) {
		THROW("read()", errmsg());
	}

	append(end_offset - len, '\n'); // New offset
	append(to_hex(buff)); // Data
}

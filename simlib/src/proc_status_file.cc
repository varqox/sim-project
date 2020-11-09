#include "simlib/proc_status_file.hh"
#include "simlib/debug.hh"
#include "simlib/fd_pread_buff.hh"

FileDescriptor open_proc_status(pid_t tid) noexcept {
	constexpr StringView prefix = "/proc/";
	auto tid_str = to_string(tid);
	constexpr StringView suffix = "/status";
	StaticCStringBuff<
		prefix.size() + decltype(tid_str)::max_size() + suffix.size()>
		path;
	size_t pos = 0;
	for (auto s : {prefix, StringView{tid_str}, suffix}) {
		for (auto c : s) {
			path[pos++] = c;
		}
	}
	path[pos] = '\0';
	path.len_ = pos;

	return FileDescriptor{open(path.data(), O_RDONLY | O_CLOEXEC)};
}

InplaceBuff<24>
field_from_proc_status(int proc_status_fd, StringView field_name) {
	InplaceBuff<24> res;
	FdPreadBuff fbuff(proc_status_fd);
	auto get_char_or_eof = [&] {
		auto c_opt = fbuff.get_char();
		if (not c_opt) {
			THROW("read()", errmsg());
		}
		return *c_opt;
	};
	auto get_char = [&] {
		int c = get_char_or_eof();
		if (c == EOF) {
			THROW("field \"", field_name, "\" was not found");
		}
		return c;
	};
	auto skip_to_newline = [&] {
		int c{};
		do {
			c = get_char();
		} while (c != '\n');
	};
	for (;;) {
	next_iter:;
		for (char fc : field_name) {
			int c = get_char();
			if (c != fc) {
				skip_to_newline();
				goto next_iter;
			}
		}
		int c = get_char();
		if (c != ':') {
			skip_to_newline();
			continue;
		}
		// Skip white space
		do {
			c = get_char_or_eof();
		} while (c == '\t' or c == '\n');
		// Retrieve the field contents
		while (c != '\n' and c != EOF) {
			res.append(static_cast<char>(c));
			c = get_char_or_eof();
		}
		return res;
	}
}

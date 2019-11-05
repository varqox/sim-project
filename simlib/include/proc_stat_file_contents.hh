#pragma once

#include "string_view.hh"

#include <vector>

struct ProcStatFileContents {
private:
	std::string contents_;
	std::vector<StringView> fields_;

	explicit ProcStatFileContents(std::string stat_file_contents);

public:
	ProcStatFileContents(const ProcStatFileContents&) = delete;
	ProcStatFileContents(ProcStatFileContents&&) noexcept = default;
	ProcStatFileContents& operator=(const ProcStatFileContents&) = delete;
	ProcStatFileContents& operator=(ProcStatFileContents&&) noexcept = default;

	static ProcStatFileContents
	from_proc_stat_contents(std::string stat_file_contents) {
		return ProcStatFileContents {stat_file_contents};
	}

	// Returns ProcStatFileContents of /proc/@p pid/stat
	static ProcStatFileContents get(pid_t pid);

	StringView field(size_t no) const noexcept { return fields_[no]; }

	auto fields_no() const noexcept { return fields_.size(); }
};

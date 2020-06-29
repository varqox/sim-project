#pragma once

#include "simlib/debug.hh"
#include "simlib/file_descriptor.hh"
#include "simlib/string_traits.hh"

#include <unistd.h>

class TemporaryFile {
	std::string path_; // absolute path

public:
	/// Does NOT create a temporary file
	TemporaryFile() = default;

	/// The last six characters of template must be "XXXXXX" and these are
	/// replaced with a string that makes the filename unique.
	explicit TemporaryFile(std::string templ) {
		throw_assert(has_suffix(templ, "XXXXXX") &&
		             "this is needed by mkstemp");
		FileDescriptor fd(mkstemp(templ.data()));
		if (not fd.is_open()) {
			THROW("mkstemp() failed", errmsg());
		}
		path_ = std::move(templ);
	}

	TemporaryFile(const TemporaryFile&) = delete;
	TemporaryFile& operator=(const TemporaryFile&) = delete;
	TemporaryFile(TemporaryFile&&) noexcept = default;

	TemporaryFile& operator=(TemporaryFile&& tf) noexcept {
		if (is_open()) {
			unlink(path_.c_str());
		}

		path_ = std::move(tf.path_);
		return *this;
	}

	~TemporaryFile() {
		if (is_open()) {
			unlink(path_.c_str());
		}
	}

	[[nodiscard]] bool is_open() const noexcept { return not path_.empty(); }

	[[nodiscard]] const std::string& path() const noexcept { return path_; }
};

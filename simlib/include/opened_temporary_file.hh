#pragma once

#include "debug.hh"
#include "file_descriptor.hh"
#include "string_traits.hh"

#include <unistd.h>

class OpenedTemporaryFile {
	std::string path_; // absolute path
	FileDescriptor fd_;

public:
	/// Does NOT create a temporary file
	OpenedTemporaryFile() = default;

	/// The last six characters of template must be "XXXXXX" and these are
	/// replaced with a string that makes the filename unique.
	explicit OpenedTemporaryFile(std::string templ) {
		throw_assert(has_suffix(templ, "XXXXXX") &&
		             "this is needed by mkstemp");
		fd_ = mkstemp(templ.data());
		if (not fd_.is_open())
			THROW("mkstemp() failed", errmsg());
		path_ = std::move(templ);
	}

	OpenedTemporaryFile(const OpenedTemporaryFile&) = delete;
	OpenedTemporaryFile& operator=(const OpenedTemporaryFile&) = delete;
	OpenedTemporaryFile(OpenedTemporaryFile&&) noexcept = default;

	OpenedTemporaryFile& operator=(OpenedTemporaryFile&& tf) noexcept {
		if (is_open())
			unlink(path_.c_str());

		path_ = std::move(tf.path_);
		fd_ = std::move(tf.fd_);

		return *this;
	}

	~OpenedTemporaryFile() {
		if (is_open())
			unlink(path_.c_str());
	}

	operator const FileDescriptor&() const { return fd_; }

	operator int() const { return fd_; }

	bool is_open() const noexcept { return fd_.is_open(); }

	const std::string& path() const noexcept { return path_; }
};

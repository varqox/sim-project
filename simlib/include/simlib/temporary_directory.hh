#pragma once

#include "simlib/debug.hh"
#include "simlib/file_path.hh"

#include <memory>

class TemporaryDirectory {
private:
	std::string path_; // absolute path
	std::unique_ptr<char[]> name_;

public:
	TemporaryDirectory() = default; // Does NOT create a temporary directory

	// @p templ has to end with "XXXXXX" (6 characters 'X')
	explicit TemporaryDirectory(FilePath templ);

	TemporaryDirectory(const TemporaryDirectory&) = delete;
	TemporaryDirectory(TemporaryDirectory&&) noexcept = default;
	TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
	// NOLINTNEXTLINE(performance-noexcept-move-constructor)
	TemporaryDirectory& operator=(TemporaryDirectory&& td);

	~TemporaryDirectory();

	// Returns true if object holds a real temporary directory
	[[nodiscard]] bool exists() const noexcept { return (name_ != nullptr); }

	// Directory name (from constructor parameter) with trailing '/'
	[[nodiscard]] const char* name() const noexcept { return name_.get(); }

	// Directory name (from constructor parameter) with trailing '/'
	[[nodiscard]] std::string sname() const { return name_.get(); }

	// Directory absolute path with trailing '/'
	[[nodiscard]] const std::string& path() const noexcept { return path_; }
};

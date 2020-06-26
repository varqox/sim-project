#pragma once

#include "simlib/debug.hh"
#include "simlib/file_path.hh"

#include <dirent.h>

// Encapsulates directory object DIR
class Directory {
	DIR* dir_;

public:
	explicit Directory(DIR* dir = nullptr) noexcept : dir_(dir) {}

	explicit Directory(FilePath pathname) noexcept : dir_(opendir(pathname)) {}

	Directory(const Directory&) = delete;

	Directory(Directory&& d) noexcept : dir_(d.release()) {}

	Directory& operator=(const Directory&) = delete;

	Directory& operator=(Directory&& d) noexcept {
		reset(d.release());
		return *this;
	}

	Directory& operator=(DIR* d) noexcept {
		reset(d);
		return *this;
	}

	bool is_open() const noexcept { return (dir_ != nullptr); }

	operator DIR*() const noexcept { return dir_; }

	void rewind() noexcept {
		if (is_open())
			rewinddir(dir_);
	}

	[[nodiscard]] DIR* release() noexcept {
		DIR* d = dir_;
		dir_ = nullptr;
		return d;
	}

	void reset(DIR* d) noexcept {
		if (dir_)
			(void)closedir(dir_);
		dir_ = d;
	}

	void close() noexcept {
		if (dir_) {
			(void)closedir(dir_);
			dir_ = nullptr;
		}
	}

	Directory& reopen(FilePath pathname) noexcept {
		reset(opendir(pathname));
		return *this;
	}

	~Directory() {
		if (dir_)
			(void)closedir(dir_);
	}
};

/**
 * @brief Calls @p func on every component of the @p dir other than "." and
 *   ".."
 *
 * @param dir path to directory or directory object, readdir(3) is used on it so
     one may want to save its pos via telldir(3) and use seekdir(3) after the
     call or just rewinddir(3) after the call
 * @param func function to call on every component (other than "." and ".."),
 *   it should take one argument - dirent*, if it return sth convertible to
 *   false the lookup will break
 */
template <class DirType, class Func, class ErrFunc>
void for_each_dir_component(DirType&& dir, Func&& func,
                            ErrFunc&& readdir_failed) {
	static_assert(std::is_convertible_v<DirType, FilePath> or
	              std::is_convertible_v<DirType, DIR*>);
	static_assert(std::is_invocable_r_v<bool, Func, dirent*> or
	              std::is_invocable_v<Func, dirent*>);

	if constexpr (not std::is_convertible_v<DirType, DIR*>) {
		Directory directory {dir};
		if (!directory)
			THROW("opendir()", errmsg());

		return for_each_dir_component(directory, std::forward<Func>(func),
		                              std::forward<ErrFunc>(readdir_failed));

	} else {
		dirent* file;
		for (;;) {
			errno = 0;
			file = readdir(dir);
			if (file == nullptr) {
				if (errno == 0)
					return; // No more entries

				readdir_failed();
				return;
			}

			if (strcmp(file->d_name, ".") and strcmp(file->d_name, "..")) {
				if constexpr (std::is_constructible_v<decltype(func(file)),
				                                      bool>) {
					if (not func(file))
						return;
				} else {
					func(file);
				}
			}
		}
	}
}

template <class DirType, class Func>
auto for_each_dir_component(DirType&& dir, Func&& func) {
	static_assert(std::is_convertible_v<DirType, FilePath> or
	              std::is_convertible_v<DirType, DIR*>);
	static_assert(std::is_invocable_r_v<bool, Func, dirent*> or
	              std::is_invocable_v<Func, dirent*>);
	return for_each_dir_component(std::forward<DirType>(dir),
	                              std::forward<Func>(func),
	                              [] { THROW("readdir()", errmsg()); });
}

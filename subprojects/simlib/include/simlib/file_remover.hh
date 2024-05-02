#pragma once

#include <exception>
#include <simlib/errmsg.hh>
#include <simlib/macros/throw.hh>
#include <string>
#include <unistd.h>

class FileRemover {
    std::string path;
    int uncaught_exceptions = std::uncaught_exceptions();

public:
    FileRemover() noexcept = default;

    explicit FileRemover(std::string path) noexcept : path{std::move(path)} {}

    ~FileRemover() noexcept(false) {
        if (!path.empty() && unlink(path.c_str()) && errno != ENOENT &&
            uncaught_exceptions == std::uncaught_exceptions())
        {
            THROW("unlink(", path, ')', errmsg());
        }
    }

    FileRemover(const FileRemover&) = delete;
    FileRemover& operator=(const FileRemover&) = delete;

    FileRemover(FileRemover&& other) noexcept : path{std::move(other.path)} {}

    // NOLINTNEXTLINE
    FileRemover& operator=(FileRemover&& other) {
        if (!path.empty() && unlink(path.c_str()) && errno != ENOENT) {
            THROW("unlink(", path, ')', errmsg());
        }
        path = std::move(other.path);
        return *this;
    }

    void cancel() noexcept { path.clear(); }
};

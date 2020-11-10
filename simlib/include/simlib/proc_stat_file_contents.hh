#pragma once

#include "simlib/string_view.hh"

#include <cassert>
#include <utility>
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

    ~ProcStatFileContents() = default;

    static ProcStatFileContents from_proc_stat_contents(std::string stat_file_contents) {
        return ProcStatFileContents{std::move(stat_file_contents)};
    }

    // Returns ProcStatFileContents of /proc/@p pid/stat
    static ProcStatFileContents get(pid_t pid);

    [[nodiscard]] auto fields_no() const noexcept { return fields_.size(); }

    [[nodiscard]] StringView field(size_t no) const noexcept {
        assert(no < fields_no());
        return fields_[no];
    }
};

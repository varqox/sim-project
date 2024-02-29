#pragma once

#include <simlib/string_view.hh>

class ArgvParser {
    uint argc_;
    const char* const* argv_;

public:
    ArgvParser(int argc, const char* const* argv) : argc_(std::max(argc, 0)), argv_(argv) {}

    ArgvParser(const ArgvParser&) = default;
    ArgvParser(ArgvParser&&) noexcept = default;
    ArgvParser& operator=(const ArgvParser&) = default;
    ArgvParser& operator=(ArgvParser&&) noexcept = default;

    ~ArgvParser() = default;

    [[nodiscard]] uint size() const noexcept { return argc_; }

    CStringView operator[](uint n) const noexcept {
        return (n < argc_ ? CStringView(argv_[n]) : CStringView());
    }

    [[nodiscard]] CStringView next() const noexcept { return operator[](0); }

    CStringView extract_next() noexcept {
        if (argc_ > 0) {
            --argc_;
            return CStringView(argv_++[0]);
        }
        return {};
    }
};

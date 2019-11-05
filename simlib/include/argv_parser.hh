#pragma once

#include "string_view.hh"

class ArgvParser {
	uint argc_;
	char** argv_;

public:
	ArgvParser(int argc, char** argv)
	   : argc_(std::max(argc - 1, 0)), argv_(argv + 1) {}

	ArgvParser(const ArgvParser&) = default;
	ArgvParser(ArgvParser&&) noexcept = default;
	ArgvParser& operator=(const ArgvParser&) = default;
	ArgvParser& operator=(ArgvParser&&) noexcept = default;

	~ArgvParser() = default;

	uint size() const noexcept { return argc_; }

	CStringView operator[](uint n) const noexcept {
		return (n < argc_ ? CStringView(argv_[n]) : CStringView());
	}

	CStringView next() const noexcept { return operator[](0); }

	CStringView extract_next() noexcept {
		if (argc_ > 0) {
			--argc_;
			return CStringView(argv_++[0]);
		} else {
			return {};
		}
	}
};

#pragma once

#include "simlib/argv_parser.hh"
#include "simlib/string_view.hh"

#include <functional>
#include <set>
#include <string>

template <class T, class U>
bool is_subsequence(T&& subseqence, U&& sequence) noexcept;

bool matches_pattern(StringView pattern, StringView str) noexcept;

std::set<std::string>
files_matching_patterns(std::function<bool(StringView)> file_qualifies, ArgvParser cmd_args);

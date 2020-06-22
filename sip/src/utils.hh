#pragma once

#include <set>
#include <simlib/argv_parser.hh>
#include <simlib/string_view.hh>
#include <string>

template <class T, class U>
bool is_subsequence(T&& subseqence, U&& sequence) noexcept;

bool matches_pattern(StringView pattern, StringView str) noexcept;

std::set<std::string> files_matching_patterns(ArgvParser args);

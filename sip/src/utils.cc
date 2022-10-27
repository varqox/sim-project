#include "utils.hh"

#include <simlib/sim/problem_package.hh>
#include <type_traits>

using std::set;
using std::string;
using std::vector;

template <class T, class U>
bool is_subsequence(T&& subseqence, U&& sequence) noexcept {
    if (subseqence.empty()) {
        return true;
    }

    size_t i = 0;
    for (auto const& x : sequence) {
        if (x == subseqence[i] and ++i == subseqence.size()) {
            return true;
        }
    }

    return false;
}

bool matches_pattern(StringView pattern, StringView str) noexcept {
    // Match as a subsequence whole str if pattern contains '.'
    if (pattern.find('.') != StringView::npos) {
        return is_subsequence(pattern, str);
    }

    // Otherwise match as a subsequence the part of str before last '.'
    auto pos = str.rfind('.');
    if (pos == StringView::npos) {
        pos = str.size();
    }

    return is_subsequence(pattern, str.substring(0, pos));
}

set<string> files_matching_patterns(
        std::function<bool(StringView)> file_qualifies, ArgvParser cmd_args) {
    set<string> res;
    sim::PackageContents pc;
    pc.load_from_directory(".");

    vector<StringView> patterns;
    while (cmd_args.size() > 0) {
        if (auto pattern = cmd_args.extract_next(); pc.exists(pattern) and file_qualifies(pattern))
        {
            res.emplace(pattern.to_string()); // Exact path to file
        } else {
            patterns.emplace_back(pattern);
        }
    }

    if (not patterns.empty()) {
        pc.for_each_with_prefix("", [&](StringView file) {
            if (file_qualifies(file)) {
                for (auto& pattern : patterns) {
                    if (matches_pattern(pattern, file)) {
                        res.emplace(file.to_string());
                        break;
                    }
                }
            }
        });
    }

    return res;
}

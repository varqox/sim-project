#pragma once

#include <simlib/aho_corasick.hh>

namespace sim {

class CppSyntaxHighlighter {
    AhoCorasick aho;

public:
    CppSyntaxHighlighter();

    CppSyntaxHighlighter(const CppSyntaxHighlighter&) = default;
    CppSyntaxHighlighter(CppSyntaxHighlighter&&) = default;
    CppSyntaxHighlighter& operator=(const CppSyntaxHighlighter&) = default;
    CppSyntaxHighlighter& operator=(CppSyntaxHighlighter&&) = default;
    ~CppSyntaxHighlighter() = default;

    // Returns html table containing coloured code @p input
    std::string operator()(CStringView input) const;
};

} // namespace sim

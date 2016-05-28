#pragma once

#include <simlib/aho_corasick.h>

class CppSyntaxHighlighter {
	AhoCorasick aho;

public:
	CppSyntaxHighlighter();

	CppSyntaxHighlighter(const CppSyntaxHighlighter& csh) : aho(csh.aho) {}

	CppSyntaxHighlighter(CppSyntaxHighlighter&& ch) : aho(std::move(ch.aho)) {}

	CppSyntaxHighlighter& operator=(const CppSyntaxHighlighter& csh) {
		aho = csh.aho;
		return *this;
	}

	CppSyntaxHighlighter& operator=(CppSyntaxHighlighter&& csh) {
		aho = std::move(csh.aho);
		return *this;
	}

	// Returns html table containing coloured code @p str
	std::string operator()(const StringView& str) const;
};

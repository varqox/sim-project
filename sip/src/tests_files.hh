#pragma once

#include <optional>
#include <simlib/avl_dict.hh>
#include <simlib/sim/problem_package.hh>

struct TestsFiles {
	struct Test {
		std::optional<StringView> in, out;
		Test(StringView test_path) {
			if (has_suffix(test_path, ".in"))
				in = test_path;
			else
				out = test_path;
		}
	};

	sim::PackageContents pc;
	AVLDictMap<StringView, Test> tests;

	TestsFiles();
};

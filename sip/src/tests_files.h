#pragma once

#include <simlib/optional.h>
#include <simlib/sim/problem_package.h>

struct TestsFiles {
	struct Test {
		Optional<StringView> in, out;
		Test(StringView test_path) {
			if (hasSuffix(test_path, ".in"))
				in = test_path;
			else
				out = test_path;
		}
	};

	sim::PackageContents pc;
	AVLDictMap<StringView, Test> tests;

	TestsFiles();
};

#pragma once

#include "../include/filesystem.h"
#include "../include/memory.h"

extern UniquePtr<TemporaryDirectory> tmp_dir;
extern bool GEN_OUT, VALIDATE_OUT, USE_CONF, FORCE_AUTO_LIMIT;
extern unsigned VERBOSITY; // 0 - quiet, 1 - normal, 2 or more - verbose
extern unsigned long long MEMORY_LIMIT; // in bytes
extern unsigned long long HARD_TIME_LIMIT, TIME_LIMIT; // in usec
extern UniquePtr<directory_tree::node> package_tree_root;

/**
 * @brief ProblemConfig holds SIM package config
 * @details Holds problem name, problem tag, problem statement, checker,
 * solution, memory limit and grouped tests with time limit for each
 */
class ProblemConfig {
public:
	std::string name, tag, statement, checker, solution;
	unsigned long long memory_limit; // in kB

	/**
	 * @brief Holds test
	 */
	struct Test {
		std::string name;
		unsigned long long time_limit; // in usec
	};

	/**
	 * @brief Test comparator
	 * @details Used to compare two Test objects
	 */
	struct TestCmp {
		bool operator()(const Test& a, const Test& b) const {
			return a.name < b.name;
		}
	};

	/**
	 * @brief Holds group of tests
	 * @details Holds group of tests and points for them
	 */
	struct Group {
		std::vector<Test> tests;
		long long points;
	};

	std::vector<Group> test_groups;

	ProblemConfig() : name(), tag(), statement(), checker(), solution(),
		memory_limit(0), test_groups() {}

	~ProblemConfig() {}

	/**
	 * @brief Dumps object to string
	 * @return dumped config (can be placed in file)
	 */
	std::string dump();
};

extern ProblemConfig conf_cfg;

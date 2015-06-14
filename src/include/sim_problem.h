#pragma once

#include <string>
#include <vector>

/**
 * @brief ProblemConfig holds SIM package config
 * @details Holds problem name, problem tag, problem statement, checker,
 * solution, memory limit and grouped tests with time limit for each one
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

	/**
	 * @brief loads and validates config file from problem package
	 * @p package_path
	 * @details Validates problem config (memory limits, tests existence etc.)
	 *
	 * @param package_path Path to problem package main directory
	 * @param verbosity verbosity level: 0 - quiet (no output),
	 * 1 - (errors only), 2 or more - verbose mode
	 * @return 0 on success, -1 on error
	 */
	int loadConfig(std::string package_path, unsigned verbosity);
};

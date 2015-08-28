#pragma once

#include "string.h"

#include <vector>

/**
 * @brief ProblemConfig holds SIM package config
 * @details Holds problem name, problem tag, problem statement, checker,
 * solution, memory limit and grouped tests, with time limit for each one
 */
class ProblemConfig {
public:
	std::string name, tag, statement, checker, main_solution;
	std::vector<std::string> solutions;
	unsigned long long memory_limit; // in kB

	/**
	 * @brief Holds test
	 */
	struct Test {
		std::string name;
		unsigned long long time_limit; // in usec
	};

	/**
	 * @brief Holds group of tests
	 * @details Holds group of tests and their point information
	 */
	struct Group {
		std::vector<Test> tests;
		long long points;
	};

	std::vector<Group> test_groups;

	ProblemConfig() : name(), tag(), statement(), checker(), main_solution(),
		solutions(), memory_limit(0), test_groups() {}

	~ProblemConfig() {}

	/**
	 * @brief Dumps object to string
	 * @return dumped config (can be placed in file)
	 */
	std::string dump() const;

	/**
	 * @brief loads and validates config file from problem package
	 * @p package_path
	 * @details Validates problem config (memory limits, tests existence etc.)
	 * loaded via ConfigFile
	 *
	 * Fields:
	 *   - name
	 *   - tag (optional, max length = 4)
	 *   - memory_limit (in kB)
	 *   - checker (optional)
	 *   - statement (optional)
	 *   - solutions (optional)
	 *   - main_solution (optional)
	 *   - tests (optional)
	 *
	 * @param package_path path to problem package main directory
	 *
	 * @errors Throw an exception if an error occurs
	 */
	void loadConfig(std::string package_path);

	/**
	 * @brief Converts string @p str that it can be safely placed in problem
	 * config
	 * @details If @p str is string literal then it will be returned, if @p str
	 * contain '\n' then it will be escaped as double quoted string, otherwise
	 * as single quoted string. For example:
	 * "foo-bar" -> "foo-bar" (string literal)
	 * "line: 1\nabc d E\n" -> "\"line: 1\\nabc d E\\n\"" (double quoted string)
	 * "My awesome text" -> "'My awesome text'" (single quoted string)
	 * "\\\\\\\\" -> '\\\\' (single quoted string)
	 *
	 * @param str input string
	 * @return escaped (and quoted) string
	 */
	static std::string makeSafeString(const StringView& str);
};

/**
 * @brief Makes tag from @p str
 * @details Tag is lower of 3 first not blank characters from @p str
 *
 * @param str string to make tag from it
 * @return tag
 */
std::string getTag(const std::string& str);

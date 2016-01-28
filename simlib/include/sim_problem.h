#pragma once

#include "string.h"

#include <vector>

/**
 * @brief ProblemConfig holds SIM package config
 * @details Holds problem name, problem tag, problem statement, checker,
 *   solution, memory limit and grouped tests, with time limit for each one
 */
class ProblemConfig {
public:
	std::string name, tag, statement, checker, main_solution;
	std::vector<std::string> solutions;
	unsigned long long memory_limit = 0; // in kB

	/**
	 * @brief Holds test
	 */
	struct Test {
		std::string name;
		unsigned long long time_limit; // in usec

		explicit Test(const std::string& n = "", unsigned long long tl = 0)
			: name(n), time_limit(tl) {}
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

	ProblemConfig() = default;

	ProblemConfig(const ProblemConfig&) = default;

	ProblemConfig(ProblemConfig&&) = default;

	ProblemConfig& operator=(const ProblemConfig&) = default;

	ProblemConfig& operator=(ProblemConfig&&) = default;

	~ProblemConfig() = default;

	/**
	 * @brief Dumps object to string
	 *
	 * @return dumped config (which can be placed in file)
	 */
	std::string dump() const;

	/**
	 * @brief Loads and validates config file from problem package
	 *   @p package_path
	 * @details Validates problem config (memory limits, tests specification
	 *   etc.) loaded via ConfigFile
	 *
	 *   Fields:
	 *     - name
	 *     - tag (optional, max length = 4)
	 *     - memory_limit (in kB)
	 *     - checker (optional)
	 *     - statement (optional)
	 *     - solutions (optional)
	 *     - main_solution (optional)
	 *     - tests (optional)
	 *
	 * @param package_path path to problem package main directory
	 *
	 * @return Warnings - every inconsistency with package config format
	 *
	 * @errors May throw an exception if loading error occurs (see
	 *   ConfigFile::loadConfigFromFile())
	 */
	std::vector<std::string> looselyLoadConfig(std::string package_path)
		noexcept(false);

	/**
	 * @brief Loads and validates config file from problem package
	 *   @p package_path
	 * @details Uses looselyLoadConfig() but also validates tests, checker and
	 *   solutions existence
	 *
	 * @param package_path path to problem package main directory
	 *
	 * @errors Throw an exception if any error occurs or any inconsistency with
	 *   package config format is found
	 */
	void loadConfig(std::string package_path) noexcept(false);


	/**
	 * @brief Converts string @p str that it can be safely placed in problem
	 *   config
	 * @details If @p str is string literal then it will be returned, if @p str
	 *   contain '\n' then it will be escaped as double quoted string, otherwise
	 *   as single quoted string. For example:
	 *   "foo-bar" -> "foo-bar" (string literal)
	 *   "line: 1\nab d E\n" -> "\"line: 1\\nab d E\\n\"" (double quoted string)
	 *   "My awesome text" -> "'My awesome text'" (single quoted string)
	 *   "\\\\\\\\" -> '\\\\' (single quoted string)
	 *
	 * @param str input string
	 * @param escape_unprintable whether escape unprintable via
	 *   ConfigFile::safeDoubleQoutedString()
	 *
	 * @return escaped (and quoted) string
	 */
	static std::string makeSafeString(const StringView& str,
		bool escape_unprintable = false);
};

/**
 * @brief Makes tag from @p str
 * @details Tag is lower of 3 first not blank characters from @p str
 *
 * @param str string to make tag from it
 *
 * @return tag
 */
std::string getTag(const std::string& str);

#pragma once

#include "../string.h"

#include <vector>

namespace sim {

/**
 * @brief Simfile holds SIM package configuration file
 * @details Holds problem name, problem tag, problem statement, checker,
 *   solution, memory limit and grouped tests, with time limit for each one
 */
class Simfile {
public:
	std::string name, tag, statement, checker, main_solution;
	std::vector<std::string> solutions;
	unsigned long long memory_limit = 0; // in KiB

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

	Simfile() = default;

	Simfile(const Simfile&) = default;

	Simfile(Simfile&&) = default;

	Simfile& operator=(const Simfile&) = default;

	Simfile& operator=(Simfile&&) = default;

	~Simfile() = default;

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
	 *     - memory_limit (in KB)
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
	std::vector<std::string> loadFrom(std::string package_path)
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
	void loadFromAndValidate(std::string package_path) noexcept(false);
};

/**
 * @brief Makes tag from @p str
 * @details Tag is lower of 3 first not blank characters from @p str
 *
 * @param str string to make tag from it
 *
 * @return tag
 */
std::string makeTag(const std::string& str);

/**
 * @brief Obtains checker output (truncated if too long)
 *
 * @param fd file descriptor of file to which checker wrote via stdout
 * @param max_length maximum length of the returned string
 *
 * @return checker output, truncated if too long
 *
 * @errors Throws an exception of type std::runtime_error with appropriate
 *   message
 */
std::string obtainCheckerOutput(int fd, size_t max_length) noexcept(false);

} // namespace sim

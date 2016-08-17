#pragma once

#include "../config_file.h"

namespace sim {

/**
 * @brief Simfile holds SIM package configuration file
 */
class Simfile {
public:
	std::string name, tag, statement, checker;
	std::vector<std::string> solutions;
	uint64_t global_mem_limit = 0; // in bytes

	/**
	 * @brief Holds a test
	 */
	struct Test {
		std::string name, in, out; // in, out - paths to test input and output
		uint64_t time_limit; // in usec
		uint64_t memory_limit; // in bytes

		explicit Test(const std::string& n = "", uint64_t tl = 0,
				uint64_t ml = 0)
			: name(n), time_limit(tl), memory_limit(ml) {}
	};

	/**
	 * @brief Holds a group of tests
	 * @details Holds a group of tests and their score information
	 */
	struct TestGroup {
		std::vector<Test> tests;
		int64_t score;
	};

	std::vector<TestGroup> tgroups;
	ConfigFile config;

	Simfile() = default;

	// TODO: comment
	Simfile(std::string simfile) {
		config.addVars("name", "tag", "checker", "statement", "solutions",
			"memory_limit", "limits", "scoring", "tests_files");
		config.loadConfigFromString(std::move(simfile));
	}

	Simfile(const Simfile&) = default;
	Simfile(Simfile&&) = default;
	Simfile& operator=(const Simfile&) = default;
	Simfile& operator=(Simfile&&) = default;

	~Simfile() = default;

	/**
	 * @brief Dumps object to string
	 *
	 * @return dumped config (which can be placed in a file)
	 */
	std::string dump() const;

	/**
	 * @brief Loads and validates config file from problem package
	 *   @p package_path
	 *
	 * @param package_path path to problem package main directory
	 *
	 * @return Warnings - every inconsistency with package config format
	 *
	 * @errors May throw an exception if a loading error occurs (see
	 *   ConfigFile::loadConfigFromString())
	 */

	/**
	 * @brief Loads and validates Simfile from string
	 * @details Validates problem config (memory limits, tests specification
	 *   etc.) loaded via ConfigFile
	 *
	 *   Fields:
	 *     - name
	 *     - tag (optional, max length = 4)
	 *     - memory_limit (in KB, has to be > 0)
	 *     - checker (optional)
	 *     - statement (optional)
	 *     - solutions (optional, the first one is the main solution)
	 *     - main_solution (optional)
	 *     - limits (optional)
	 *     - scoring (optional)
	 *     - tests_files (optional)
	 *
	 * @param simfile Contents of the Simfile file
	 *
	 * @return Warnings // TODO: for sure ???
	 *
	 * @errors May throw an exception from ConfigFile::loadConfigFromString(),
	 *   or an instance of std::runtime_error if any inconsistency with Simfile
	 *   format is detected
	 */
	// std::vector<std::string> loadFromString(std::string simfile);

	// TODO: comment
	void loadName();

	// TODO: comment
	void loadTag();

	// TODO: comment
	void loadChecker();

	// TODO: comment
	void loadStatement();

	// TODO: comment
	void loadSolutions();

	// TODO: comment
	void loadTests();

	// TODO: comment
	void loadTestsWithFiles();

	// TODO: comment
	void validateFiles(const StringView& package_path) const;

	struct TestNameComparator {
		/**
		 * @brief Splits @p test_name into gid (group id) and tid (test id)
		 * @details e.g. "test1abc" -> ("1", "abc")
		 *
		 * @param test_name string from which gid and tid will be extracted
		 *
		 * @return (gid, tid)
		 */
		static inline std::pair<StringView, StringView>
			split(StringView test_name) noexcept
		{
			StringView tid = test_name.extractTrailing(isalpha);
			StringView gid = test_name.extractTrailing(isdigit);
			return {gid, tid};
		}

		bool operator()(StringView a, StringView b) const {
			auto x = split(std::move(a)), y = split(std::move(b));
			// tid == "ocen" behaves the same as gid == "0"
			if (x.second == "ocen") {
				if (y.second == "ocen")
					return StrNumCompare()(x.first, y.first);

				return (y.first != "0");
			}
			if (y.second == "ocen")
				return (x.first == "0");

			return (x.first == y.first ?
				x.second < y.second : StrNumCompare()(x.first, y.first));
		}
	};
};

/**
 * @brief Makes a tag from @p str
 * @details Tag is made of lowered 3 (at most) first characters of @p str for
 *   which isgraph(3) != 0
 *
 * @param str string to make the tag from
 *
 * @return tag
 */
inline std::string makeTag(const StringView& str) {
	std::string tag;
	for (char c : str)
		if (isgraph(c) && (tag += ::tolower(c)).size() == 3)
			break;
	return tag;
}

} // namespace sim

#pragma once

#include "../config_file.h"

/// # Simfile - Sim package configuration file
/// Simfile is a ConfigFile file, so the syntax is as in ConfigFile
///
/// ## Example:
/// ```sh
/// name: Simple Package                       # Problem name
/// tag: sim                                   # Problem tag
/// statement: doc/sim.pdf                     # Path to statement file
/// checker: check/checker.cpp                 # Path to checker source file
/// solutions: [prog/sim.cpp, prog/sim1.cpp]   # Paths to solutions source files
///                                            # The first solution is the main
///                                            # solution
/// memory_limit: 65536           # Global memory limit in KB (optional)
/// limits: [                     # Limits array
///         # Group 0
///         sim0a 1            # Format: <test name> <time limit> [memory limit]
///         sim0b 1.01         # Time limit in seconds, memory limit in KB
///         sim1ocen 2 32768   # Memory limit is optional if global memory limit
///         sim2ocen 3         # is set.
///                            # Tests may appear in arbitrary order
///         # Group 1
///         sim1a 1
///         sim1b 1
///
///         # Group 2
///         sim2a 2
///         sim2b 2
///
///         # Group 3
///         sim3a 3
///         sim3b 3
///
///         # Group 4
///         sim4 5 32768
/// ]
/// scoring: [                    # Scoring of the tests group (optional)
///         0 0      # Format: <group id> <score>
///         1 20     # Score is a signed integer value
///         2 30     # Groups may appear in arbitrary order
///         3 25
///         4 25
/// ]
/// tests_files: [                # Input and output files of the tests
///                            # Format: <test name> <in file> <out file>
///         sim0a in/sim0a.in out/sim0a.out
///         sim0b in/sim0b.in out/sim0b.out
///         sim1ocen in/sim1ocen.in out/sim1ocen.out
///         sim2ocen in/sim2ocen.in out/sim2ocen.out
///         sim1a in/sim1a.in out/sim1a.out
///         sim1b in/sim1b.in out/sim1b.out
///         sim2a in/sim2a.in out/sim2a.out
///         sim2b in/sim2b.in out/sim2b.out
///         sim3a in/sim3a.in out/sim3a.out
///         sim3b in/sim3b.in out/sim3b.out
///         sim4 in/sim4.in out/sim4.out
/// ]
/// ```

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

	/**
	 * @brief Loads needed variables from @p simfile
	 * @details Uses ConfigFile::loadConfigFromString
	 *
	 * @param simfile Simfile file contents
	 *
	 * @errors May throw from ConfigFile::loadConfigFromString
	 */
	Simfile(std::string simfile) {
		config.addVars("name", "tag", "checker", "statement", "solutions",
			"memory_limit", "limits", "scoring", "tests_files");
		config.loadConfigFromString(std::move(simfile));
	}

	Simfile(const Simfile&) = default;
	Simfile(Simfile&&) noexcept = default;
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
	 * @brief Loads problem name
	 * @details Fields:
	 *   - name (problem name)
	 *
	 *   @errors Throws an exception of type std::runtime_error if any
	 *     validation error occurs
	 */
	void loadName();

	/**
	 * @brief Loads problem tag
	 * @details Fields:
	 *   - tag (problem tag)
	 *
	 *   @errors Throws an exception of type std::runtime_error if any
	 *     validation error occurs
	 */
	void loadTag();

	/**
	 * @brief Loads path to checker source file
	 * @details Fields:
	 *   - checker (path to checker source file)
	 *
	 *   @errors Throws an exception of type std::runtime_error if any
	 *     validation error occurs
	 */
	void loadChecker();

	/**
	 * @brief Loads path to statement
	 * @details Fields:
	 *   - statement (path to statement)
	 *
	 *   @errors Throws an exception of type std::runtime_error if any
	 *     validation error occurs
	 */
	void loadStatement();

	/**
	 * @brief Loads paths to solutions (the first one is the main solution)
	 * @details Fields:
	 *   - solutions (array of paths to source files of the)
	 *
	 *   @errors Throws an exception of type std::runtime_error if any
	 *     validation error occurs
	 */
	void loadSolutions();

	/**
	 * @brief Loads tests, their limits and scoring
	 * @details Fields:
	 *   - memory_limit (optional global memory limit [KB], if specified then
	 *     memory limit in `limits` variable is optional)
	 *   - limits (array of tests limits: time [seconds] and memory [KB])
	 *   - scoring (optional array of scoring of the tests groups)
	 *
	 *   @errors Throws an exception of type std::runtime_error if any
	 *     validation error occurs
	 */
	void loadTests();

	/**
	 * @brief Loads tests, their limits, scoring and files
	 * @details Fields are identical to these of loadTests(), with addition of:
	 *   - tests_files (array of the tests' input and output files)
	 *
	 *   @errors Throws an exception of type std::runtime_error if any
	 *     validation error occurs
	 */
	void loadTestsWithFiles();

	/**
	 * @brief Validates all previously loaded files
	 * @details Validates all files loaded by calling methods: loadChecker(),
	 *   loadStatement(), loadSolutions(), loadTestsWithFiles(). All files are
	 *   loaded safely, so that no file is outside of the  package (any path
	 *   cannot go outside)
	 *
	 * @param package_path path of the package main directory, used as package
	 *   root directory during the validation
	 *
	 *   @errors Throws an exception of type std::runtime_error if any
	 *     validation error occurs
	 */
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

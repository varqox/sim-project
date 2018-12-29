#pragma once

#include "../config_file.h"
#include "../optional.h"

/// # Simfile - Sim package configuration file
/// Simfile is a ConfigFile file, so the syntax is as in ConfigFile
///
/// ## Example:
/// ```sh
/// name: Simple Package                       # Problem name
/// label: sim                                 # Problem label (usually a shortened name)
/// statement: doc/sim.pdf                     # Path to statement file
/// checker: check/checker.cpp                 # Path to checker source file
/// solutions: [prog/sim.cpp, prog/sim1.cpp]   # Paths to solutions' source files
///                                            # The first solution is the main
///                                            # solution
/// memory_limit: 64              # Global memory limit in MB (optional)
/// time_limit: 3.14              # Global time limit in seconds (optional)
/// limits: [                     # Limits array
///         # Group 0
///         sim0a 1            # Format: <test name> <time limit> [memory limit]
///         sim0b 1.01         # Time limit in seconds, memory limit in MB
///         sim1ocen 2 32      # Memory limit is optional if the global memory limit
///         sim2ocen 3         # is set on every test.
///                            # Tests may appear in an arbitrary order
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
///         sim4 5 32
/// ]
/// scoring: [                    # Scoring of the tests group (optional)
///         0 0      # Format: <group id> <score>
///         1 20     # Score is a signed integer value
///         2 30     # Groups may appear in an arbitrary order
///         3 25
///         4 25
/// ]
/// tests_files: [                # Tests' input and output files (optional)
///                               # Format: <test name> <in file> <out file>
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
	std::string name, label, statement;
	Optional<std::string> checker; // std::nullopt if default checker should be used
	std::vector<std::string> solutions;
	uint64_t global_mem_limit = 0; // in bytes

	/**
	 * @brief Holds a test
	 */
	struct Test {
		std::string name, in, out; // in, out - paths to test's input and output
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

	std::vector<TestGroup> tgroups; // Sorted by gid

private:
	ConfigFile config;

	friend class Conver; // It needs access to the parsing methods

public:
	Simfile() = default;

	/**
	 * @brief Loads needed variables from @p simfile_contents
	 * @details Uses ConfigFile::loadConfigFromString()
	 *
	 * @param simfile_contents Simfile file contents
	 *
	 * @errors May throw from ConfigFile::loadConfigFromString()
	 */
	Simfile(std::string simfile_contents) {
		config.addVars("name", "label", "checker", "statement", "solutions",
			"memory_limit", "limits", "scoring", "tests_files");
		config.loadConfigFromString(std::move(simfile_contents));
	}

	Simfile(const Simfile&) = default;
	Simfile(Simfile&&) noexcept = default;
	Simfile& operator=(const Simfile&) = default;
	Simfile& operator=(Simfile&&) = default;

	~Simfile() = default;

	const ConfigFile& configFile() const { return config; }

	/**
	 * @brief Dumps object to string
	 *
	 * @return dumped config (which can be placed in a file)
	 */
	std::string dump() const;

	/**
	 * @brief Dumps value of scoring variable to string
	 *
	 * @return dumped value of scoring variable (which can be placed in config
	 *   file as scoring value)
	 */
	std::string dump_scoring_value() const;

	/**
	 * @brief Dumps value of limits variable to string
	 *
	 * @return dumped value of limits variable (which can be placed in config
	 *   file as limits value)
	 */
	std::string dump_limits_value() const;

	/**
	 * @brief Loads problem's name
	 * @details Fields:
	 *   - name (problem's name)
	 *
	 * @errors Throws an exception of type std::runtime_error if any
	 *   validation error occurs
	 */
	void loadName();

	/**
	 * @brief Loads problem's label
	 * @details Fields:
	 *   - label (problem's label)
	 *
	 * @errors Throws an exception of type std::runtime_error if any
	 *   validation error occurs
	 */
	void loadLabel();

	/**
	 * @brief Loads path to checker source file (if there is any)
	 * @details Fields:
	 *   - checker (path to checker source file or std::nullopt if default
	 *       checker should be used)
	 *
	 * @errors Throws an exception of type std::runtime_error if any
	 *   validation error occurs
	 */
	void loadChecker();

	/**
	 * @brief Loads path to statement
	 * @details Fields:
	 *   - statement (path to statement)
	 *
	 * @errors Throws an exception of type std::runtime_error if any
	 *   validation error occurs
	 */
	void loadStatement();

	/**
	 * @brief Loads paths to solutions (the first one is the main solution)
	 * @details Fields:
	 *   - solutions (array of paths to source files of the)
	 *
	 * @errors Throws an exception of type std::runtime_error if any
	 *   validation error occurs
	 */
	void loadSolutions();

private:

#if __cplusplus > 201402L
#warning "Since C++17 std::optional should be used for memory limit"
#endif

	/**
	 * @brief Parses the item of the variable "limits"
	 * @param item - the item to parse
	 * @return (test name, time limit [usec], memory limit [byte] or 0 if not given)
	 */
	static std::tuple<StringView, uint64_t, uint64_t> parseLimitsItem(StringView item);

	/**
	 * @brief Parses the item of the variable "scoring"
	 * @param item - the item to parse
	 * @return (tests group id, group's score)
	 */
	static std::tuple<StringView, int64_t> parseScoringItem(StringView item);

public:

	/**
	 * @brief Loads tests, their limits and scoring
	 * @details Fields:
	 *   - memory_limit (optional global memory limit [MB], if specified then
	 *     glogal_mem_limit > 0 and memory limit in `limits` variable is
	 *     optional)
	 *   - limits (array of tests limits: time [seconds] and memory [MB])
	 *   - scoring (optional array of scoring of the tests groups)
	 *
	 * @errors Throws an exception of type std::runtime_error if any
	 *   validation error occurs
	 */
	void loadTests();

	/**
	 * @brief Loads only the global memory limit
	 * @details Fields:
	 *   - memory_limit (optional global memory limit [MB], if specified then
	 *     glogal_mem_limit > 0 )
	 *
	 * @errors Throws an exception of type std::runtime_error if any
	 *   validation error occurs
	 */
	void loadGlobalMemoryLimitOnly();

private:
	/**
	 * @brief Parses the item of the variable "tests_files"
	 * @param item - the item to parse
	 * @return (test name, path to input file, path to output file)
	 */
	static std::tuple<StringView, StringView, StringView>
	parseTestFilesItem(StringView item);

public:

	/**
	 * @brief Loads tests files (input and output files)
	 * @details Fields:
	 *   - tests_files (array of the tests' input and output files)
	 *
	 * @errors Throws an exception of type std::runtime_error if any
	 *   validation error occurs
	 */
	void loadTestsFiles();

	/**
	 * @brief Loads tests, their limits, scoring and files
	 * @details Fields are identical to these of loadTests(), with addition of:
	 *   - tests_files (array of the tests' input and output files)
	 *
	 * @errors Throws an exception of type std::runtime_error if any
	 *   validation error occurs
	 */
	void loadTestsWithFiles() {
		loadTests();
		loadTestsFiles();
	}

	/**
	 * @brief Loads everything = the whole Simfile
	 */
	void loadAll() {
		loadName();
		loadLabel();
		loadChecker();
		loadStatement();
		loadSolutions();
		loadTestsWithFiles();
	}

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
	void validateFiles(StringView package_path) const;

	struct TestNameComparator {
		struct SplitResult {
			StringView gid; // Group id
			StringView tid; // Test id
		};

		/**
		 * @brief Splits @p test_name into gid (group id) and tid (test id)
		 * @details e.g. "test1abc" -> ("1", "abc")
		 *
		 * @param test_name string from which gid and tid will be extracted
		 *
		 * @return (gid, tid)
		 */
		static inline SplitResult split(StringView test_name) noexcept {
			SplitResult res;
			res.tid = test_name.extractTrailing(::isalpha);
			res.gid = test_name.extractTrailing(::isdigit);
			return res;
		}

		bool operator()(StringView a, StringView b) const {
			auto x = split(std::move(a)), y = split(std::move(b));
			// tid == "ocen" behaves the same as gid == "0"
			if (x.tid == "ocen") {
				if (y.tid == "ocen")
					return StrNumCompare()(x.gid, y.gid);

				y.gid.removeLeading('0');
				return (not y.gid.empty()); // true iff y.gid was not equal to 0
			}
			if (y.tid == "ocen") {
				x.gid.removeLeading('0');
				return x.gid.empty(); // true iff x.gid was equal to 0
			}

			return (x.gid == y.gid ?
				x.tid < y.tid : StrNumCompare()(x.gid, y.gid));
		}
	};
};

/**
 * @brief Makes an label from @p str
 * @details Sbbreviation is made of lowered 3 (at most) first characters of
 *   @p str for which isgraph(3) != 0
 *
 * @param str string to make the label
 *
 * @return label
 */
inline std::string shortenName(StringView str) {
	std::string label;
	for (char c : str)
		if (isgraph(c) && (label += ::tolower(c)).size() == 3)
			break;
	return label;
}

} // namespace sim

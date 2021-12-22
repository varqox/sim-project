#pragma once

#include "simlib/config_file.hh"
#include "simlib/ctype.hh"
#include "simlib/string_view.hh"

#include <chrono>
#include <optional>
#include <utility>

/// # Simfile - Sim package configuration file
/// Simfile is a ConfigFile file, so the syntax is the same as in the
/// ConfigFile
///
/// ## Example:
/// ```sh
/// name: Simple Package                     # Problem name
/// label: sim                               # Problem label (usually a
///                                          #   shortened name)
/// interactive: false                       # Whether the problem is
///                                          #   interactive or not
///                                          #   (optional - default false)
/// statement: doc/sim.pdf                   # Path to statement file
/// checker: check/checker.cpp               # Path to checker source file
///                                          #   (optional if the problem
///                                          #     is not interactive),
///                                          #   if not set, the default
///                                          #   checker will be used
/// solutions: [prog/sim.cpp, prog/sim1.cpp] # Paths to solutions' source files.
///                                          #   The first solution is the main
///                                          #   solution
/// memory_limit: 64           # Global memory limit in MiB (optional)
/// limits: [                  # Limits array
///         # Group 0
///         sim0a 1        # Format: <test name> <time limit> [memory limit]
///         sim0b 1.01     # Time limit in seconds, memory limit in MiB
///         sim1ocen 2 32  # Individual memory limit is optional if the global
///                        #   memory limit is set.
///         sim2ocen 3     # Tests may appear in an arbitrary order
///
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
/// scoring: [                 # Scoring of the tests groups (optional)
///         0 0      # Format: <group id> <score>
///         1 20     # Score is a signed integer value
///         2 30     # Groups may appear in an arbitrary order
///         3 25
///         4 25
/// ]
/// tests_files: [         # Tests' input and output files (optional)
///                        # Format: <test name> <in file> <out file - present
///                        #   only if the package is not interactive>
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
 * @brief Simfile holds Sim package configuration file
 */
class Simfile {
public:
    std::optional<std::string> name; // Is unset only if not loaded
    std::optional<std::string> label; // Is unset only if not loaded
    std::optional<std::string> statement; // Is unset only if not loaded
    bool interactive = false;
    std::optional<std::string> checker; // std::nullopt if default checker should be used
    std::vector<std::string> solutions;
    std::optional<uint64_t> global_mem_limit; // in bytes

    /**
     * @brief Holds a test
     */
    struct Test {
        std::string name, in; // in - path to the test's input file
        std::optional<std::string> out; // out - path to the test's output file (set
                                        //   only if the package is not interactive)
        std::chrono::nanoseconds time_limit;
        uint64_t memory_limit; // in bytes

        explicit Test(std::string n = "",
                std::chrono::nanoseconds tl = std::chrono::nanoseconds(0), uint64_t ml = 0)
        : name(std::move(n))
        , time_limit(tl)
        , memory_limit(ml) {}
    };

    /**
     * @brief Holds a group of tests
     * @details Holds a group of tests and their score information
     */
    struct TestGroup {
        std::vector<Test> tests;
        int64_t score{};
    };

    std::vector<TestGroup> tgroups; // Sorted by gid

private:
    ConfigFile config;

    friend class Conver; // It needs access to the parsing methods

public:
    Simfile() = default;

    /**
     * @brief Loads needed variables from @p simfile_contents
     * @details Uses ConfigFile::load_config_from_string()
     * @param simfile_contents Simfile file contents
     *
     * @errors May throw from ConfigFile::load_config_from_string()
     */
    explicit Simfile(std::string simfile_contents) {
        config.add_vars("name", "label", "interactive", "checker", "statement", "solutions",
                "memory_limit", "limits", "scoring", "tests_files");
        config.load_config_from_string(std::move(simfile_contents));
    }

    Simfile(const Simfile&) = default;
    Simfile(Simfile&&) noexcept = default;
    Simfile& operator=(const Simfile&) = default;
    Simfile& operator=(Simfile&&) = default;

    ~Simfile() = default;

    [[nodiscard]] const ConfigFile& config_file() const { return config; }
    /**
     * @brief Dumps object to string
     *
     * @return dumped config (which can be placed in a file)
     */
    [[nodiscard]] std::string dump() const;

    /**
     * @brief Dumps value of scoring variable to string
     *
     * @return dumped value of scoring variable (which can be placed in config
     *   file as scoring value)
     */
    [[nodiscard]] std::string dump_scoring_value() const;

    /**
     * @brief Dumps value of limits variable to string
     *
     * @return dumped value of limits variable (which can be placed in config
     *   file as limits value)
     */
    [[nodiscard]] std::string dump_limits_value() const;

    /**
     * @brief Loads problem's name
     * @details Fields:
     *   - name (problem's name)
     *
     * @errors Throws an exception of type std::runtime_error if any
     *   validation error occurs
     */
    void load_name();
    /**
     * @brief Loads problem's label
     * @details Fields:
     *   - label (problem's label)
     *
     * @errors Throws an exception of type std::runtime_error if any
     *   validation error occurs
     */
    void load_label();
    /**
     * @brief Loads problem's interactive setting
     * @details Fields:
     *   - interactive (whether the problem is interactive or not)
     *
     * @errors Throws an exception of type std::runtime_error if any
     *   validation error occurs
     */
    void load_interactive();
    /**
     * @brief Loads path to checker source file (if there is any)
     * @details Fields:
     *   - checker (path to checker source file or std::nullopt if default
     *       checker should be used)
     *   - interactive (whether the problem is interactive or not)
     *
     * @errors Throws an exception of type std::runtime_error if any
     *   validation error occurs
     */
    void load_checker();
    /**
     * @brief Loads path to statement
     * @details Fields:
     *   - statement (path to statement)
     *
     * @errors Throws an exception of type std::runtime_error if any
     *   validation error occurs
     */
    void load_statement();
    /**
     * @brief Loads paths to solutions (the first one is the main solution)
     * @details Fields:
     *   - solutions (array of paths to source files of the)
     *
     * @errors Throws an exception of type std::runtime_error if any
     *   validation error occurs
     */
    void load_solutions();

private:
    /**
     * @brief Parses the item of the variable "limits"
     * @param item - the item to parse
     * @return (test name, time limit [usec], memory limit [byte])
     */
    static std::tuple<StringView, std::chrono::nanoseconds, std::optional<uint64_t>>
    parse_limits_item(StringView item);
    /**
     * @brief Parses the item of the variable "scoring"
     * @param item - the item to parse
     * @return (tests group id, group's score)
     */
    static std::tuple<StringView, int64_t> parse_scoring_item(StringView item);

public:
    /**
     * @brief Loads tests, their limits and scoring
     * @details Fields:
     *   - memory_limit (optional global memory limit [MiB], if specified then
     *     glogal_mem_limit > 0 and memory limit in `limits` variable is
     *     optional)
     *   - limits (array of tests limits: time [seconds] and memory [MiB])
     *   - scoring (optional array of scoring of the tests groups)
     *
     * @errors Throws an exception of type std::runtime_error if any
     *   validation error occurs
     */
    void load_tests();
    /**
     * @brief Loads only the global memory limit
     * @details Fields:
     *   - memory_limit (optional global memory limit [MiB], if specified then
     *     glogal_mem_limit > 0 )
     *
     * @errors Throws an exception of type std::runtime_error if any
     *   validation error occurs
     */
    void load_global_memory_limit_only();

private:
    /**
     * @brief Parses the item of the variable "tests_files"
     * @detail each of the returned field may be empty - validation occurs only
     *   for excessive columns
     * @param item - the item to parse
     * @return (test name, path to input file, path to output file)
     */
    static std::tuple<StringView, StringView, StringView> parse_test_files_item(
            StringView item);

public:
    /**
     * @brief Loads tests files (input and output files)
     * @details Fields:
     *   - tests_files (array of the tests' input and output files)
     *
     * @errors Throws an exception of type std::runtime_error if any
     *   validation error occurs
     */
    void load_tests_files();
    /**
     * @brief Loads tests, their limits, scoring and files
     * @details Fields are identical to these of load_tests(), with addition of:
     *   - tests_files (array of the tests' input and output files)
     *
     * @errors Throws an exception of type std::runtime_error if any
     *   validation error occurs
     */
    void load_tests_with_files() {
        load_tests();
        load_tests_files();
    }

    /**
     * @brief Loads everything = the whole Simfile
     */
    void load_all() {
        load_name();
        load_label();
        load_checker();
        load_statement();
        load_solutions();
        load_tests_with_files();
    }

    /**
     * @brief Validates all previously loaded files
     * @details Validates all files loaded by calling methods: load_checker(),
     *   load_statement(), load_solutions(), load_tests_with_files(). All files
     *   are loaded safely, so that no file is outside of the  package (any path
     *   cannot go outside)
     * @param package_path path of the package main directory, used as package
     *   root directory during the validation
     *
     *   @errors Throws an exception of type std::runtime_error if any
     *     validation error occurs
     */
    void validate_files(StringView package_path) const;
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
            res.tid = test_name.extract_trailing(is_alpha<char>);
            res.gid = test_name.extract_trailing(is_digit<char>);
            return res;
        }

        bool operator()(StringView a, StringView b) const {
            auto x = split(a);
            auto y = split(b);
            auto normalize_gid = [](StringView& gid) {
                while (gid.size() > 1 and gid[0] == '0') {
                    gid.remove_prefix(1);
                }
            };
            normalize_gid(x.gid);
            normalize_gid(x.gid);
            // tid == "ocen" behaves the same as gid == "0"
            if (x.tid == "ocen" and y.tid == "ocen") {
                return StrVersionCompare()(x.gid, y.gid);
            }
            if (x.tid == "ocen") {
                x.gid = "0";
            }
            if (y.tid == "ocen") {
                y.gid = "0";
            }
            return (x.gid == y.gid ? x.tid < y.tid : StrVersionCompare()(x.gid, y.gid));
        }
    };
};

/**
 * @brief Makes an label from @p str
 * @details Sbbreviation is made of lowered 3 (at most) first characters of
 *   @p str for which is_graph(3) != 0
 *
 * @param str string to make the label
 *
 * @return label
 */
inline std::string shorten_name(const StringView& str) {
    std::string label;
    for (auto c : str) {
        if (is_graph(c) && (label += to_lower(c)).size() == 3) {
            break;
        }
    }
    return label;
}

} // namespace sim

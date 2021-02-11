#include "src/sipfile.hh"
#include "simlib/file_info.hh"
#include "simlib/path.hh"
#include "simlib/sim/judge_worker.hh"
#include "simlib/sim/problem_package.hh"
#include "simlib/sim/simfile.hh"
#include "simlib/string_transform.hh"
#include "src/sip_error.hh"
#include "src/utils.hh"

#include <optional>

using std::not_fn;

// Macros because of string concentration in compile-time (in C++11 it is hard
// to achieve in the other way and this macros are pretty more readable than
// some template meta-programming code that concentrates string literals. Also,
// the macros are used only locally, so after all, they are not so evil...)
#define CHECK_IF_ARR(var, name)                                 \
    if (!(var).is_array() && (var).is_set()) {                  \
        throw SipError("Sipfile: variable `" name "` has to be" \
                       " specified as an array");               \
    }
#define CHECK_IF_NOT_ARR(var, name)                             \
    if ((var).is_array()) {                                     \
        throw SipError("Sipfile: variable `" name "` cannot be" \
                       " specified as an array");               \
    }

void Sipfile::load_default_time_limit() {
    STACK_UNWINDING_MARK;

    auto&& var = config["default_time_limit"];
    CHECK_IF_NOT_ARR(var, "default_time_limit");

    if (var.as_string().empty()) {
        throw SipError("Sipfile: missing default_time_limit");
    }

    auto opt = str2num<double>(var.as_string());
    if (not opt) {
        throw SipError("Sipfile: invalid default time limit");
    }
    if (*opt <= 0) {
        throw SipError("Sipfile: default time limit has to be grater than 0");
    }

    using std::chrono::duration;
    using std::chrono::duration_cast;
    using std::chrono_literals::operator""ns;
    using std::chrono::nanoseconds;

    default_time_limit = duration_cast<nanoseconds>(duration<double>(*opt) + 0.5ns);
    if (default_time_limit == 0ns) {
        throw SipError("Sipfile: default time limit is to small - after rounding it is "
                       "equal to 0 nanoseconds, but it has to be at least 1 nanosecond");
    }
}

void Sipfile::load_base_seed(bool warn) {
    STACK_UNWINDING_MARK;

    auto&& var = config["base_seed"];
    CHECK_IF_NOT_ARR(var, "base_seed");

    if (var.as_string().empty()) {
        if (warn) {
            log_warning("Sipfile: missing base_seed -> setting it to 0. To "
                        "change the base seed run: sip reseed");
        }
        base_seed = 0;
        return;
    }

    auto opt = str2num<decltype(base_seed)>(var.as_string());
    if (not opt) {
        throw SipError("Sipfile: invalid base seed");
    }
    base_seed = *opt;
}

template <class Func>
static void for_each_test_in_range(StringView test_range, Func&& callback) {
    STACK_UNWINDING_MARK;

    auto hypen = test_range.find('-');
    if (hypen == StringView::npos) {
        callback(test_range);
        return;
    }

    if (hypen + 1 == test_range.size()) {
        throw SipError("invalid test range (trailing hypen): ", test_range);
    }

    StringView begin = test_range.substring(0, hypen);
    StringView end = test_range.substring(hypen + 1);

    auto begin_test = sim::Simfile::TestNameComparator::split(begin);
    auto end_test = sim::Simfile::TestNameComparator::split(end);
    // The part before group id
    StringView prefix =
        begin.substring(0, begin.size() - begin_test.gid.size() - begin_test.tid.size());
    {
        // Allow e.g. test4a-5c
        StringView end_prefix =
            end.substring(0, end.size() - end_test.gid.size() - end_test.tid.size());
        if (not end_prefix.empty() and end_prefix != prefix) {
            throw SipError(
                "invalid test range (test prefix of the end test is not empty "
                "and does not match the begin test prefix): ",
                test_range);
        }
    }

    auto begin_gid_opt = str2num<uint64_t>(begin_test.gid);
    if (not begin_gid_opt) {
        throw SipError("invalid test range (group id is too big): ", test_range);
    }

    uint64_t begin_gid = begin_gid_opt.value();
    uint64_t end_gid = 0;

    if (end_test.gid.empty()) { // Allow e.g. 4a-d
        end_gid = begin_gid;
    } else {
        auto end_gid_opt = str2num<uint64_t>(end_test.gid);
        if (not end_gid_opt) {
            throw SipError("invalid test range (group id is too big): ", test_range);
        }

        end_gid = end_gid_opt.value();
    }

    std::string begin_tid = to_lower(begin_test.tid.to_string());
    std::string end_tid = to_lower(end_test.tid.to_string());

    // Allow e.g. 4-8c
    if (begin_tid.empty()) {
        begin_tid = end_tid;
    }

    if (begin_tid.size() != end_tid.size()) {
        throw SipError("invalid test range (test IDs have different length): ", test_range);
    }

    if (begin_tid > end_tid) {
        throw SipError(
            "invalid test range (begin test ID is greater than end"
            " test ID): ",
            test_range);
    }

    // Increments tid by one
    auto inc_tid = [](auto& tid) {
        if (tid.size == 0) {
            // Have to produce sth greater than "" to end inner loop below
            tid.append('a');
            return;
        }

        ++tid.back();
        for (int i = tid.size - 1; i > 0 and tid[i] > 'z'; --i) {
            tid[i] -= ('z' - 'a' + 1);
            ++tid[i - 1];
        }
    };

    for (auto gid = begin_gid; gid <= end_gid; ++gid) {
        for (InplaceBuff<8> tid(begin_tid); tid <= end_tid; inc_tid(tid)) {
            callback(intentional_unsafe_string_view(concat(prefix, gid, tid)));
        }
    }
}

void Sipfile::load_static_tests() {
    STACK_UNWINDING_MARK;

    auto static_tests_var = config.get_var("static");
    CHECK_IF_ARR(static_tests_var, "static");

    static_tests.emplace(); // Set if std::nullopt, clear otherwise
    for (StringView entry : static_tests_var.as_array()) {
        entry.extract_leading(is_space<char>);
        StringView test_range = entry.extract_leading(not_fn(is_space<char>));
        entry.extract_leading(is_space<char>);
        if (not entry.empty()) {
            log_warning(
                "Sipfile (static): ignoring invalid suffix: `", entry,
                "` of the entry with test range: ", test_range);
        }

        for_each_test_in_range(test_range, [&](StringView test) {
            auto [_, inserted] = static_tests->emplace(test);
            if (not inserted) {
                log_warning(
                    "Sipfile (static): test `", test,
                    "` is specified"
                    " in more than one test range");
            }
        });
    }
}

/// @p pkg_contents should contain all files that @p pattern will be watched
/// with
static InplaceBuff<32>
matching_generator(StringView pattern, const sim::PackageContents& pkg_contents) {
    STACK_UNWINDING_MARK;

    // Generators specified by path always match
    if (pkg_contents.exists(pattern)) {
        return InplaceBuff<32>(pattern);
    }

    std::optional<StringView> res;
    pkg_contents.for_each_with_prefix("utils/", [&](StringView file) {
        if (matches_pattern(pattern, file) and sim::is_source(file)) {
            if (res.has_value()) {
                throw SipError(
                    "Sipfile: specified generator `", pattern,
                    "` matches more than one file: `", res.value(), "` and `", file, '`');
            }

            res = file;
        }
    });

    if (res.has_value()) {
        return InplaceBuff<32>(*res);
    }

    log_warning(
        "Sipfile (gen): no file in utils/ matches specified generator: `", pattern,
        "`. It will be treated as a shell command.\n"
        "  To remove this warning you have to choose one of the following "
        "options:\n"
        "    1. Provide full path to the generator file e.g. "
        "generators/gen1.cpp\n"
        "    2. If it is a shell command, prefix the generator with sh: - e.g. "
        "sh:echo");
    return concat<32>("sh:", pattern);
}

void Sipfile::load_gen_tests() {
    STACK_UNWINDING_MARK;

    auto gen_tests_var = config.get_var("gen");
    CHECK_IF_ARR(gen_tests_var, "gen");

    sim::PackageContents pkg_contents;
    pkg_contents.load_from_directory(".");
    pkg_contents.remove_with_prefix("utils/cache/");

    gen_tests.emplace(); // Set if std::nullopt, clear otherwise
    for (StringView entry : gen_tests_var.as_array()) {
        // Test range
        entry.extract_leading(is_space<char>);
        StringView test_range = entry.extract_leading(not_fn(is_space<char>));
        entry.extract_leading(is_space<char>);
        // Generator
        StringView specified_generator = entry.extract_leading(not_fn(is_space<char>));
        entry.extract_leading(is_space<char>);
        // Generator arguments
        entry.extract_trailing(is_space<char>);
        StringView generator_args = entry;

        if (specified_generator.empty()) {
            throw SipError(
                "Sipfile (gen): missing generator for the test range `", test_range, '`');
        }

        // Match generator
        InplaceBuff<32> generator;
        if (has_prefix(specified_generator, "sh:") or
            access(path_absolute(specified_generator), F_OK) == 0)
        {
            generator = specified_generator;
        } else {
            generator = matching_generator(specified_generator, pkg_contents);
        }

        for_each_test_in_range(test_range, [&](StringView test) {
            auto [_, inserted] = gen_tests->emplace(test, generator, generator_args);
            if (not inserted) {
                throw SipError(
                    "Sipfile (gen): test `", test,
                    "` is specified in more than one test range");
            }
        });
    }
}

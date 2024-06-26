#include <bits/stdc++.h>
using namespace std;

constexpr inline char nl = '\n';

namespace args_parser {
int argc;
const char** argv;
} // namespace args_parser

// Automatically checks that argc is big enough
template <class T = int64_t>
T arg(int n) {
    assert(n > 0);
    if (n >= args_parser::argc) {
        (void)fputs("Too few arguments specified\n", stderr);
        _exit(1);
    }
    auto argn = args_parser::argv[n];
    auto argn_end = argn + strlen(argn);
    if constexpr (std::is_integral_v<T>) {
        T res;
        if (auto [p, ec] = from_chars(argn, argn_end, res); ec == std::errc() and p == argn_end) {
            return res;
        }
    }
    // If integer conversion failed, try a floating one
    if constexpr (std::is_arithmetic_v<T>) {
        errno = 0;
        char* ptr = nullptr;
        long double res = ::strtold(argn, &ptr);
        if (errno == 0 and ptr != argn and ptr == argn_end) {
            return res;
        }
        (void)fprintf(
            stderr,
            "Invalid argument no %i: cannot convert to %s\n",
            n,
            std::is_integral_v<T> ? "integer" : "floating point number"
        );
        _exit(1);
    } else {
        return T{argn};
    }
}

std::string_view test_name() {
    static auto test_name = getenv("SIP_TEST_NAME");
    if (not test_name) {
        (void)fputs("SIP_TEST_NAME environment variable is not set\n", stderr);
        _exit(1);
    }
    return test_name;
}

std::mt19937_64& generator() {
    static std::mt19937_64 generator{[] {
        auto seed_str = getenv("SIP_TEST_SEED");
        if (not seed_str) {
            (void)fputs("SIP_TEST_SEED environment variable is not set\n", stderr);
            _exit(1);
        }
        uint64_t seed{};
        if (auto [p, ec] = from_chars(seed_str, seed_str + strlen(seed_str), seed);
            ec != std::errc())
        {
            (void)fputs("SIP_TEST_SEED environment variable is not a valid integer\n", stderr);
            _exit(1);
        }
        return seed;
    }()};
    return generator;
}

inline int64_t rd(int64_t min_val, int64_t max_val) {
    assert(min_val <= max_val);
    return std::uniform_int_distribution{min_val, max_val}(generator());
}

int main(int main_argc, const char** main_argv) {
    args_parser::argc = main_argc;
    args_parser::argv = main_argv;
    // main_argv[0] command (ignored)
    // main_argv[1..] generator arguments
    // stdout = a valid test input for solution
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int n = rd(arg(1), arg(2));
    cout << n << nl;
}

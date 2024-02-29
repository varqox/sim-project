#include <cstdint>
#include <gtest/gtest.h>
#include <map>
#include <sim/cpp_syntax_highlighter.hh>
#include <simlib/concat.hh>
#include <simlib/directory.hh>
#include <simlib/file_contents.hh>
#include <simlib/path.hh>
#include <simlib/process.hh>
#include <simlib/string_compare.hh>
#include <simlib/string_traits.hh>
#include <simlib/string_view.hh>

using std::string;
using std::vector;

static constexpr bool regenerate_outs = false;

vector<string> collect_tests(const string& tests_dir) {
    std::map<string, int8_t, StrNumCompare> test_file_num;
    for_each_dir_component(tests_dir, [&](dirent* file) {
        StringView file_name = file->d_name;
        bool test_file = false;
        for (StringView suffix : {".in", ".out"}) {
            if (has_suffix(file_name, suffix)) {
                ++test_file_num[file_name.substring(0, file_name.size() - suffix.size())
                                    .to_string()];
                test_file = true;
                break;
            }
        }
        if (not test_file) {
            ADD_FAILURE() << "file " << file_name << " does not belong to any test";
        }
    });
    // Choose files that have both *.in and *.out
    vector<string> res;
    for (auto&& [test_name, file_num] : test_file_num) {
        if (file_num == 2) {
            res.emplace_back(test_name);
        } else {
            ADD_FAILURE() << "test " << test_name << " has only one file";
        }
    }
    return res;
}

void test_cpp_systax_highlighter(const string& tests_dir) {
    auto tests = collect_tests(tests_dir);
    assert(has_suffix(tests_dir, "/"));
    sim::CppSyntaxHighlighter csh;
    for (const auto& test_name : tests) {
        auto csh_ans = csh(from_unsafe{get_file_contents(concat(tests_dir, test_name, ".in"))});
        auto test_out_path = concat(tests_dir, test_name, ".out");
        if (regenerate_outs) {
            put_file_contents(test_out_path, csh_ans);
        }
        auto expected_ans = get_file_contents(test_out_path);
        EXPECT_EQ(expected_ans, csh_ans) << "^ test " << test_name;
    }
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
string test_cases_dir;

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    assert(argc == 2);
    test_cases_dir = argv[1];

    return RUN_ALL_TESTS();
}

// NOLINTNEXTLINE
TEST(cpp_syntax_highlighter, syntax_highlighting) { test_cpp_systax_highlighter(test_cases_dir); }

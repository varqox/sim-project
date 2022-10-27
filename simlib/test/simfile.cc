#include <gtest/gtest.h>
#include <simlib/debug.hh>
#include <simlib/file_manip.hh>
#include <simlib/sim/simfile.hh>
#include <simlib/temporary_directory.hh>
#include <simlib/utilities.hh>

using std::array;
using std::string;
using std::vector;

// NOLINTNEXTLINE
TEST(Simfile, dump) {
    sim::Simfile sf{"name: Simple Package\n"
                    "label: sim\n"
                    "statement: doc/sim.pdf\n"
                    "checker: check/checker.cpp\n"
                    "solutions: [prog/sim.cpp, prog/sim1.cpp]\n"
                    "memory_limit: 64\n"
                    "limits: [\n"
                    "  sim0a 1\n"
                    "  sim0b 1\n"
                    "  sim1ocen 2\n"
                    "  sim2ocen 3\n"
                    "  sim1a 1\n"
                    "  sim1b 1\n"
                    "  sim2a 2\n"
                    "  sim2b 2\n"
                    "  sim3a 3\n"
                    "  sim3b 3\n"
                    "  sim4 5 32\n"
                    "]\n"
                    "scoring: [\n"
                    "  0 0\n"
                    "  1 20\n"
                    "  2 30\n"
                    "  3 25\n"
                    "  4 25\n"
                    "]\n"
                    "tests_files: [\n"
                    "  sim0a      in/sim0a.in      out/sim0a.out\n"
                    "  sim0b      in/sim0b.in      out/sim0b.out\n"
                    "  sim1ocen   in/sim1ocen.in   out/sim1ocen.out\n"
                    "  sim2ocen   in/sim2ocen.in   out/sim2ocen.out\n"
                    "  sim1a      in/sim1a.in      out/sim1a.out\n"
                    "  sim1b      in/sim1b.in      out/sim1b.out\n"
                    "  sim2a      in/sim2a.in      out/sim2a.out\n"
                    "  sim2b      in/sim2b.in      out/sim2b.out\n"
                    "  sim3a      in/sim3a.in      out/sim3a.out\n"
                    "  sim3b      in/sim3b.in      out/sim3b.out\n"
                    "  sim4       in/sim4.in       out/sim4.out\n"
                    "]\n"};

    for (int i = 1; i <= 2; ++i) {
        sf.load_name();
        sf.load_label();
        sf.load_checker();
        sf.load_statement();
        sf.load_solutions();
        sf.load_tests_with_files();

        EXPECT_EQ("name: Simple Package\n"
                  "label: sim\n"
                  "statement: doc/sim.pdf\n"
                  "checker: check/checker.cpp\n"
                  "solutions: [prog/sim.cpp, prog/sim1.cpp]\n"
                  "memory_limit: 64\n"
                  "limits: [\n"
                  "\tsim0a 1\n"
                  "\tsim0b 1\n"
                  "\tsim1ocen 2\n"
                  "\tsim2ocen 3\n"
                  "\n"
                  "\tsim1a 1\n"
                  "\tsim1b 1\n"
                  "\n"
                  "\tsim2a 2\n"
                  "\tsim2b 2\n"
                  "\n"
                  "\tsim3a 3\n"
                  "\tsim3b 3\n"
                  "\n"
                  "\tsim4 5 32\n"
                  "]\n"
                  "scoring: [\n"
                  "\t0 0\n"
                  "\t1 20\n"
                  "\t2 30\n"
                  "\t3 25\n"
                  "\t4 25\n"
                  "]\n"
                  "tests_files: [\n"
                  "\tsim0a in/sim0a.in out/sim0a.out\n"
                  "\tsim0b in/sim0b.in out/sim0b.out\n"
                  "\tsim1ocen in/sim1ocen.in out/sim1ocen.out\n"
                  "\tsim2ocen in/sim2ocen.in out/sim2ocen.out\n"
                  "\tsim1a in/sim1a.in out/sim1a.out\n"
                  "\tsim1b in/sim1b.in out/sim1b.out\n"
                  "\tsim2a in/sim2a.in out/sim2a.out\n"
                  "\tsim2b in/sim2b.in out/sim2b.out\n"
                  "\tsim3a in/sim3a.in out/sim3a.out\n"
                  "\tsim3b in/sim3b.in out/sim3b.out\n"
                  "\tsim4 in/sim4.in out/sim4.out\n"
                  "]\n",
                sf.dump())
                << "iteration: " << i;
    }

    // Omit memory limit when not necessary
    sf = sim::Simfile{"limits: [\n"
                      "  sim0 1 32\n"
                      "  sim1 1 64\n"
                      "]\n"};

    sf.load_tests();

    EXPECT_EQ("solutions: []\n"
              "limits: [\n"
              "\tsim0 1 32\n"
              "\n"
              "\tsim1 1 64\n"
              "]\n"
              "scoring: [\n"
              "\t0 0\n"
              "\t1 100\n"
              "]\n"
              "tests_files: [\n"
              "]\n",
            sf.dump());
}

// NOLINTNEXTLINE
TEST(Simfile, load_name) {
    sim::Simfile sf{"name: Problem 1 2 3"};
    // Load two times - make sure that it is safe
    for (int i = 1; i <= 2; ++i) {
        sf.load_name();
        EXPECT_EQ("Problem 1 2 3", sf.name.value()) << "iteration: " << i;
    }

    // Exceptions
    sf = sim::Simfile{""};
    EXPECT_THROW(sf.load_name(), std::runtime_error);
    sf = sim::Simfile{"name: []"};
    EXPECT_THROW(sf.load_name(), std::runtime_error);
    sf = sim::Simfile{"name:"};
    sf.load_name();
    EXPECT_EQ(sf.name.value(), "");
}

// NOLINTNEXTLINE
TEST(Simfile, load_label) {
    sim::Simfile sf{"label: Label 1 2 3"};
    // Load two times - make sure that it is safe
    for (int i = 1; i <= 2; ++i) {
        sf.load_label();
        EXPECT_EQ("Label 1 2 3", sf.label.value()) << "iteration: " << i;
    }

    // Exceptions
    sf = sim::Simfile{""};
    EXPECT_THROW(sf.load_label(), std::runtime_error);
    sf = sim::Simfile{"label: []"};
    EXPECT_THROW(sf.load_label(), std::runtime_error);
    sf = sim::Simfile{"label:"};
    sf.load_label();
    EXPECT_EQ(sf.label.value(), "");
}

// NOLINTNEXTLINE
TEST(Simfile, load_interactive) {
    sim::Simfile sf{"interactive: true"};
    // Load two times - make sure that it is safe
    for (int i = 1; i <= 2; ++i) {
        sf.load_interactive();
        EXPECT_EQ(true, sf.interactive) << "iteration: " << i;
    }

    EXPECT_EQ("interactive: true\n"
              "solutions: []\n"
              "limits: []\n"
              "tests_files: [\n"
              "]\n",
            sf.dump());

    sf = sim::Simfile{"interactive: false"};
    // Load two times - make sure that it is safe
    for (int i = 1; i <= 2; ++i) {
        sf.load_interactive();
        EXPECT_EQ(false, sf.interactive) << "iteration: " << i;
    }

    sf = sim::Simfile{""};
    sf.load_interactive();
    EXPECT_EQ(false, sf.interactive);

    sf = sim::Simfile{"interactive:"};
    sf.load_interactive();
    EXPECT_EQ(false, sf.interactive);

    EXPECT_EQ("solutions: []\n"
              "limits: []\n"
              "tests_files: [\n"
              "]\n",
            sf.dump());

    // Exceptions
    sf = sim::Simfile{"interactive: []"};
    EXPECT_THROW(sf.load_interactive(), std::runtime_error);
}

// NOLINTNEXTLINE
TEST(Simfile, load_checker) {
    sim::Simfile sf{"checker: path/to/checker"};
    // Load two times - make sure that it is safe
    for (int i = 1; i <= 2; ++i) {
        sf.load_checker();
        EXPECT_EQ("path/to/checker", sf.checker.value()) << "iteration: " << i;
    }

    // Exceptions
    sf = sim::Simfile{""};
    sf.load_checker();
    EXPECT_FALSE(sf.checker.has_value());

    sf = sim::Simfile{"checker: []"};
    EXPECT_THROW(sf.load_checker(), std::runtime_error);

    sf = sim::Simfile{"checker:"};
    sf.load_checker();
    EXPECT_FALSE(sf.checker.has_value());

    sf = sim::Simfile{"checker: ''"};
    sf.load_checker();
    EXPECT_FALSE(sf.checker.has_value());

    // Unsafe paths
    sf = sim::Simfile{"checker: ../suspicious/path"};
    sf.load_checker();
    EXPECT_EQ("suspicious/path", sf.checker.value());

    sf = sim::Simfile{"checker: ./suspicious/../../../../path/"};
    sf.load_checker();
    EXPECT_EQ("path/", sf.checker.value());
}

// NOLINTNEXTLINE
TEST(Simfile, load_statement) {
    sim::Simfile sf{"statement: path/to/statement"};
    // Load two times - make sure that it is safe
    for (int i = 1; i <= 2; ++i) {
        sf.load_statement();
        EXPECT_EQ("path/to/statement", sf.statement) << "iteration: " << i;
    }

    // Exceptions
    sf = sim::Simfile{""};
    EXPECT_THROW(sf.load_statement(), std::runtime_error);

    sf = sim::Simfile{"statement: []"};
    EXPECT_THROW(sf.load_statement(), std::runtime_error);

    sf = sim::Simfile{"statement:"};
    EXPECT_THROW(sf.load_statement(), std::runtime_error);

    // Unsafe paths
    sf = sim::Simfile{"statement: ../suspicious/path"};
    sf.load_statement();
    EXPECT_EQ("suspicious/path", sf.statement);

    sf = sim::Simfile{"statement: ./suspicious/../../../../path/"};
    sf.load_statement();
    EXPECT_EQ("path/", sf.statement);
}

// NOLINTNEXTLINE
TEST(Simfile, load_solutions) {
    using VS = vector<string>;
    sim::Simfile sf{"solutions: [sol1, sol/2]"};
    // Load two times - make sure that it is safe
    for (int i = 1; i <= 2; ++i) {
        sf.load_solutions();
        EXPECT_EQ((VS{"sol1", "sol/2"}), sf.solutions) << "iteration: " << i;
    }

    // Exceptions
    sf = sim::Simfile{""};
    EXPECT_THROW(sf.load_solutions(), std::runtime_error);

    sf = sim::Simfile{"solutions: foo"};
    EXPECT_THROW(sf.load_solutions(), std::runtime_error);

    sf = sim::Simfile{"solutions: []"};
    EXPECT_THROW(sf.load_solutions(), std::runtime_error);

    sf = sim::Simfile{"solutions:"};
    EXPECT_THROW(sf.load_solutions(), std::runtime_error);

    // Unsafe paths
    sf = sim::Simfile{"solutions: [../suspicious/path, "
                      "./suspicious/../../../../path/]"};
    sf.load_solutions();
    EXPECT_EQ((VS{"suspicious/path", "path/"}), sf.solutions);

    sf = sim::Simfile{"solutions: [./suspicious/../../../../path/, "
                      "../suspicious/path]"};
    sf.load_solutions();
    EXPECT_EQ((VS{"path/", "suspicious/path"}), sf.solutions);
}

// NOLINTNEXTLINE
TEST(Simfile, load_tests) {
    // Memory limit
    sim::Simfile sf{"memory_limit: 123\nlimits: []"};
    sf.load_tests();
    EXPECT_EQ(123 << 20, sf.global_mem_limit.value_or(0));

    // Exceptions - memory_limit
    sf = sim::Simfile{"memory_limit: []\nlimits: []"};
    EXPECT_THROW(sf.load_tests(), std::runtime_error);

    sf = sim::Simfile{"memory_limit: 0\nlimits: []"};
    EXPECT_THROW(sf.load_tests(), std::runtime_error);

    sf = sim::Simfile{"memory_limit: 18446744073709551616\nlimits: []"};
    EXPECT_THROW(sf.load_tests(), std::runtime_error);

    sf = sim::Simfile{"memory_limit: -178\nlimits: []"};
    EXPECT_THROW(sf.load_tests(), std::runtime_error);

    sf = sim::Simfile{"memory_limit: 3.14\nlimits: []"};
    EXPECT_THROW(sf.load_tests(), std::runtime_error);

    // Limits + scoring
    sf = sim::Simfile{"memory_limit: 33\n"
                      "limits: [\n"
                      "  foo1a 1.23\n"
                      "  foo2 0.01 11\n"
                      "  bar1b 2.38\n"
                      "]\n"
                      "scoring: [\n"
                      "  1 25\n"
                      "  2 75\n"
                      "]\n"};
    sf.load_tests();
    EXPECT_EQ("solutions: []\n"
              "memory_limit: 33\n"
              "limits: [\n"
              "\tfoo1a 1.23\n"
              "\tbar1b 2.38\n"
              "\n"
              "\tfoo2 0.01 11\n"
              "]\n"
              "scoring: [\n"
              "\t1 25\n"
              "\t2 75\n"
              "]\n"
              "tests_files: [\n"
              "]\n",
            sf.dump());

    // Automatic scoring
    sf = sim::Simfile{"memory_limit: 64\n"
                      "limits: [\n"
                      "  1 1\n"
                      "  2 1\n"
                      "  3 1\n"
                      "]\n"};
    // Load two times - make sure that it is safe
    for (int i = 1; i <= 2; ++i) {
        sf.load_tests();
        EXPECT_EQ("solutions: []\n"
                  "memory_limit: 64\n"
                  "limits: [\n"
                  "\t1 1\n"
                  "\n"
                  "\t2 1\n"
                  "\n"
                  "\t3 1\n"
                  "]\n"
                  "scoring: [\n"
                  "\t1 33\n"
                  "\t2 33\n"
                  "\t3 34\n"
                  "]\n"
                  "tests_files: [\n"
                  "]\n",
                sf.dump())
                << "iteration: " << i;
    }
    // Grouping ocen with 0 tests and giving them score = 0
    sf = sim::Simfile{"memory_limit: 64\n"
                      "limits: [\n"
                      "  foo0 1\n"
                      "  foo1 1\n"
                      "  foo1ocen 1\n"
                      "  foo2 1\n"
                      "  foo_a 1\n" // no group id
                      "  foo_b 1\n" // no group id
                      "]\n"};
    sf.load_tests();
    EXPECT_EQ("solutions: []\n"
              "memory_limit: 64\n"
              "limits: [\n"
              "\tfoo_a 1\n"
              "\tfoo_b 1\n"
              "\n"
              "\tfoo0 1\n"
              "\tfoo1ocen 1\n"
              "\n"
              "\tfoo1 1\n"
              "\n"
              "\tfoo2 1\n"
              "]\n"
              "scoring: [\n"
              "\t'\"\" 33'\n"
              "\t0 0\n"
              "\t1 33\n"
              "\t2 34\n"
              "]\n"
              "tests_files: [\n"
              "]\n",
            sf.dump());

    // Exceptions - limits
    sf = sim::Simfile{""};
    EXPECT_THROW(sf.load_tests(), std::runtime_error);

    sf = sim::Simfile{"limits: foo"};
    EXPECT_THROW(sf.load_tests(), std::runtime_error);

    // Exceptions - time limit
    sf = sim::Simfile{"limits: [1]"};
    EXPECT_THROW(sf.load_tests(), std::runtime_error);

    sf = sim::Simfile{"limits: [1 1..2]"};
    EXPECT_THROW(sf.load_tests(), std::runtime_error);

    sf = sim::Simfile{"limits: [1 1a2]"};
    EXPECT_THROW(sf.load_tests(), std::runtime_error);

    sf = sim::Simfile{"limits: [1 -12]"};
    EXPECT_THROW(sf.load_tests(), std::runtime_error);

    sf = sim::Simfile{"limits: [1 0]"};
    EXPECT_THROW(sf.load_tests(), std::runtime_error);

    sf = sim::Simfile{"limits: [1 0.00000049]"};
    EXPECT_THROW(sf.load_tests(), std::runtime_error);

    // Exceptions - memory limit
    sf = sim::Simfile{"limits: [1 1]"};
    EXPECT_THROW(sf.load_tests(), std::runtime_error);

    sf = sim::Simfile{"limits: [1 1 a]"};
    EXPECT_THROW(sf.load_tests(), std::runtime_error);

    sf = sim::Simfile{"limits: [1 1 -1]"};
    EXPECT_THROW(sf.load_tests(), std::runtime_error);

    sf = sim::Simfile{"limits: [1 1 0]"};
    EXPECT_THROW(sf.load_tests(), std::runtime_error);

    sf = sim::Simfile{"limits: [1 1 18446744073709551616]"};
    EXPECT_THROW(sf.load_tests(), std::runtime_error);

    sf = sim::Simfile{"limits: [1 1 3.14]"};
    EXPECT_THROW(sf.load_tests(), std::runtime_error);

    // Exceptions - scoring
    sf = sim::Simfile{"limits: []\nscoring: aaa"};
    EXPECT_THROW(sf.load_tests(), std::runtime_error);

    // Exceptions - scoring - group
    sf = sim::Simfile{"limits: []\nscoring: [a 1]"};
    EXPECT_THROW(sf.load_tests(), std::runtime_error);

    sf = sim::Simfile{"limits: []\nscoring: [1 1]"};
    EXPECT_THROW(sf.load_tests(), std::runtime_error);

    sf = sim::Simfile{"limits: [foo1a 1 2]\nscoring: [1 1, 1 1]"};
    EXPECT_THROW(sf.load_tests(), std::runtime_error);

    // Exceptions - scoring - group score
    sf = sim::Simfile{"limits: [foo1a 1 2]\nscoring: [1]"};
    EXPECT_THROW(sf.load_tests(), std::runtime_error);

    sf = sim::Simfile{"limits: [foo1a 1 2]\nscoring: [1 1.0]"};
    EXPECT_THROW(sf.load_tests(), std::runtime_error);

    sf = sim::Simfile{"limits: [foo1a 1 2]\nscoring: [1 1a2]"};
    EXPECT_THROW(sf.load_tests(), std::runtime_error);

    // Exceptions - scoring - missing scoring
    sf = sim::Simfile{"limits: [foo1a 1 2]\nscoring: []"};
    EXPECT_THROW(sf.load_tests(), std::runtime_error);

    sf = sim::Simfile{"limits: [foo1a 1 2, foo2b 3 4]\nscoring: [1 1]"};
    EXPECT_THROW(sf.load_tests(), std::runtime_error);
}

// NOLINTNEXTLINE
TEST(Simfile, load_tests_with_files) {
    sim::Simfile sf{"limits: [\n"
                    "  foo0 1 1\n"
                    "  foo1a 1 1\n"
                    "  foo1b 1 1\n"
                    "  foo2 1 1\n"
                    "]\n"
                    "tests_files: [\n"
                    "  foo0 in/foo0.in out/foo0.out\n"
                    "  foo1a foo1a.in foo1a.out\n"
                    "  foo1b ../suspicious/path/1 /../suspicious/path/2/..\n"
                    "  foo2 ./more/suspicious/../../../../path/3 "
                    "/./even/./more///suspicious/../../../../../path/4 \n"
                    "]\n"};
    // Load two times - make sure that it is safe
    for (int i = 1; i <= 2; ++i) {
        sf.load_tests_with_files();
        EXPECT_EQ("solutions: []\n"
                  "limits: [\n"
                  "\tfoo0 1 1\n"
                  "\n"
                  "\tfoo1a 1 1\n"
                  "\tfoo1b 1 1\n"
                  "\n"
                  "\tfoo2 1 1\n"
                  "]\n"
                  "scoring: [\n"
                  "\t0 0\n"
                  "\t1 50\n"
                  "\t2 50\n"
                  "]\n"
                  "tests_files: [\n"
                  "\tfoo0 in/foo0.in out/foo0.out\n"
                  "\tfoo1a foo1a.in foo1a.out\n"
                  "\tfoo1b suspicious/path/1 suspicious/path/\n"
                  "\tfoo2 path/3 path/4\n"
                  "]\n",
                sf.dump())
                << "iteration: " << i;
    }

    // Exceptions
    sf = sim::Simfile{"limits: []"};
    EXPECT_THROW(sf.load_tests_with_files(), std::runtime_error);

    sf = sim::Simfile{"limits: []\ntests_files: foo"};
    EXPECT_THROW(sf.load_tests_with_files(), std::runtime_error);

    // Exceptions - redefinition
    sf = sim::Simfile{"limits: []\ntests_files: [1 in out, 1 in out]"};
    EXPECT_THROW(sf.load_tests_with_files(), std::runtime_error);

    // Exceptions - test without files
    sf = sim::Simfile{"limits: [t1a 2 3, t2d 3 4]\ntests_files: [t1a in out]"};
    EXPECT_THROW(sf.load_tests_with_files(), std::runtime_error);

    // Exceptions - superfluous files declarations are ignored
    sf = sim::Simfile{"limits: [1 0.14 2]\ntests_files: [1 in out, 2 in out]"};
    EXPECT_NO_THROW(sf.load_tests_with_files());
}

void create_file_with_dirs(const string& path) {
    // Extract containing directory
    size_t pos = path.size();
    while (pos && path[pos - 1] != '/') {
        --pos;
    }
    // Create directory
    (void)mkdir_r(path.substr(0, pos));

    // Create file
    if (create_file(path)) {
        THROW('`', path, '`', errmsg());
    }
}

void create_files_at(string dir, const vector<string>& v) {
    if (!dir.empty() && dir.back() != '/') {
        dir += '/';
    }

    for (auto&& s : v) {
        create_file_with_dirs(dir + s);
    }
}

// NOLINTNEXTLINE
TEST(Simfile, validate_files) {
    TemporaryDirectory tmp_dir("/tmp/simlib-test.XXXXXX");

    create_files_at(tmp_dir.path(),
            {
                    "doc/statement.pdf",
                    "check/checker.c",
                    "prog/1.cpp",
                    "prog/2.cpp",
                    "prog/3.cpp",
                    "in/1.in",
                    "in/2.in",
                    "in/3a.in",
                    "in/3b.in",
                    "in/3c.in",
                    "out/1.out",
                    "out/2.out",
                    "out/3a.out",
                    "out/3b.out",
                    "out/3c.out",
            });

    sim::Simfile sf{"statement: doc/statement.pdf\n"
                    "checker: check/checker.c\n"
                    "solutions: [prog/1.cpp, prog/2.cpp, prog/3.cpp]\n"
                    "limits: [\n"
                    "  1 1 1\n"
                    "  2 1 1\n"
                    "  3a 1 1\n"
                    "  3b 1 1\n"
                    "  3c 1 1\n"
                    "]\n"
                    "tests_files: [\n"
                    " 1 in/1.in out/1.out\n"
                    " 2 in/2.in out/2.out\n"
                    " 3a in/3a.in out/3a.out\n"
                    " 3b in/3b.in out/3b.out\n"
                    " 3c in/3c.in out/3c.out\n"
                    "]\n"};

    sf.load_statement();
    sf.load_checker();
    sf.load_solutions();
    sf.load_tests_with_files();

    // Make sure that the method is const
    EXPECT_NO_THROW(const_cast<const sim::Simfile&>(sf).validate_files(tmp_dir.path()));

    // Check if not loaded values are ignored
    sf = sim::Simfile{};
    EXPECT_NO_THROW(sf.validate_files(tmp_dir.path()));

    /* Exceptions */

    // Checker - non-existing file
    sf = sim::Simfile{"checker: foo/bar"};
    sf.load_checker();
    EXPECT_THROW(sf.validate_files(tmp_dir.path()), std::runtime_error);

    // Checker - invalid type of a file
    sf = sim::Simfile{"checker: prog"};
    sf.load_checker();
    EXPECT_THROW(sf.validate_files(tmp_dir.path()), std::runtime_error);

    // Statement - non-existing file
    sf = sim::Simfile{"statement: foo/bar"};
    sf.load_statement();
    EXPECT_THROW(sf.validate_files(tmp_dir.path()), std::runtime_error);

    // Statement - invalid type of a file
    sf = sim::Simfile{"statement: prog"};
    sf.load_statement();
    EXPECT_THROW(sf.validate_files(tmp_dir.path()), std::runtime_error);

    // Solutions
    {
        array<string, 3> sols{{"prog/1.cpp", "prog/2.cpp", "prog/3.cpp"}};
        for (int i = 0; i < 3; ++i) {
            auto do_check = [&] {
                string contents{"solutions: ["};
                for (auto&& s : sols) {
                    back_insert(contents, s, ", ");
                }
                contents += ']';

                sf = sim::Simfile{contents};
                sf.load_solutions();
                EXPECT_THROW(sf.validate_files(tmp_dir.path()), std::runtime_error) << "i: " << i;
            };

            string val = std::move(sols[i]);

            // non-existing file
            sols[i] = "prog/foo.cpp";
            do_check();

            // invalid type of a file
            sols[i] = "prog/";
            do_check();

            sols[i] = std::move(val);
        }
    }

    // Tests
    {
        const array<StringView, 5> tests{{"1", "2", "3a", "3b", "3c"}};
        array<string, tests.size() * 2> files{{
                // clang-format off
            "in/1.in", "out/1.out",
            "in/2.in", "out/2.out",
            "in/3a.in", "out/3a.out",
            "in/3b.in", "out/3b.out",
            "in/3c.in", "out/3c.out",
                // clang-format on
        }};

        for (int i = 0; i < 10; ++i) {
            auto do_check = [&] {
                string contents{"limits: [1 1 1, 2 1 1, 3a 1 1, 3b 1 1, 3c 1 1]\n"
                                "tests_files: [\n"};
                static_assert(std::tuple_size<decltype(tests)>::value * 2 ==
                                std::tuple_size<decltype(files)>::value,
                        "Each test has one in and one out file");
                for (uint j = 0; j < files.size(); j += 2) {
                    back_insert(contents, tests[j >> 1], ' ', files[j], ' ', files[j + 1], '\n');
                }
                contents += "\n]";

                sf = sim::Simfile{contents};
                sf.load_tests_with_files();
                EXPECT_THROW(sf.validate_files(tmp_dir.path()), std::runtime_error) << "i: " << i;
            };

            string val = std::move(files[i]);

            // non-existing file
            files[i] = "foo/bar";
            do_check();

            // invalid type of a file
            files[i] = "prog/";
            do_check();

            files[i] = std::move(val);
        }
    }
}

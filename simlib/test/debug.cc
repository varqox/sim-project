#include "simlib/debug.hh"
#include "simlib/defer.hh"
#include "simlib/file_contents.hh"
#include "simlib/unlinked_temporary_file.hh"

#include <gtest/gtest-death-test.h>
#include <gtest/gtest.h>
#include <type_traits>

using std::string;

// NOLINTNEXTLINE
TEST(debug, throw_assert) {
    constexpr int var = 123;
    try {
        throw_assert(var == 123);
        throw_assert(var % 3 != 0);
        ADD_FAILURE();
    } catch (const std::runtime_error& e) {
        constexpr auto line = __LINE__;
        EXPECT_EQ(e.what(),
                concat(__FILE__, ':', line - 3, ": ", __PRETTY_FUNCTION__,
                        ": Assertion `var % 3 != 0` failed."));
    } catch (...) {
        ADD_FAILURE();
    }
}

template <class LoggingFunc>
static std::string intercept_logger(Logger& logger, LoggingFunc&& logging_func) {
    FileDescriptor fd{open_unlinked_tmp_file()};
    throw_assert(fd.is_open());
    std::unique_ptr<FILE, decltype(&fclose)> stream = {fdopen(fd, "r+"), fclose};
    throw_assert(stream);
    (void)fd.release(); // Now stream owns it

    FILE* logger_stream = stream.get();
    stream.reset(logger.exchange_log_stream(stream.release()));
    bool old_label = logger.label(false);
    Defer guard = [&] {
        logger.use(stream.release());
        logger.label(old_label);
    };

    logging_func();

    // Get logged data
    rewind(logger_stream);
    return get_file_contents(fileno(logger_stream));
}

// NOLINTNEXTLINE
TEST(debug, LOG_LINE_MARCO) {
    auto logged_line = intercept_logger(stdlog, [] { LOG_LINE; });
    EXPECT_EQ(concat_tostr(__FILE__, ':', __LINE__ - 1, '\n'), logged_line);
    auto logged_line2 = intercept_logger(stdlog, [] { LOG_LINE; });
    EXPECT_EQ(concat_tostr(__FILE__, ':', __LINE__ - 1, '\n'), logged_line2);
}

// NOLINTNEXTLINE
TEST(debug, THROW_MACRO) {
    try {
        THROW("a ", 1, -42, "c", '.', false, ";");
        ADD_FAILURE();
    } catch (const std::runtime_error& e) {
        constexpr auto line = __LINE__;
        EXPECT_EQ(e.what(),
                concat("a ", 1, -42, "c", '.', false, "; (thrown at ", __FILE__, ':', line - 3,
                        ')'));
    } catch (...) {
        ADD_FAILURE();
    }
}

// NOLINTNEXTLINE
TEST(debug, errmsg) {
    EXPECT_EQ(concat(" - Operation not permitted (os error ", EPERM, ')'), errmsg(EPERM));
    EXPECT_EQ(concat(" - No such file or directory (os error ", ENOENT, ')'), errmsg(ENOENT));

    errno = EPERM;
    EXPECT_EQ(concat(" - Operation not permitted (os error ", EPERM, ')'), errmsg());
    errno = ENOENT;
    EXPECT_EQ(concat(" - No such file or directory (os error ", ENOENT, ')'), errmsg());
}

namespace {
constexpr auto foo_mark_line = __LINE__ + 5;
constexpr auto foo_thrown_msg = "abc";
} // namespace

static void foo(bool throw_exception) {
    STACK_UNWINDING_MARK;
    if (throw_exception) {
        throw std::runtime_error(foo_thrown_msg);
    }
}

static void leave_stack_unwinding_mark() {
    try {
        foo(true);
        ADD_FAILURE();
    } catch (...) {
        EXPECT_GT(::stack_unwinding::StackGuard::marks_collected.size(), 0);
    }
}

static void test_errlog_catch_and_stack_unwinding_mark_normal() {
    // Earlier stack unwinding marks should not appear in ERRLOG_CATCH()
    leave_stack_unwinding_mark();

    // Check discarding earlier unwinding marks and retaining of the recent ones
    constexpr auto mark_line = __LINE__ + 2;
    try {
        STACK_UNWINDING_MARK;
        foo(true);
        ADD_FAILURE();
    } catch (const std::exception& e) {
        EXPECT_EQ(string(foo_thrown_msg), e.what());
        constexpr auto logging_line = __LINE__ + 1;
        auto logged_catch = intercept_logger(errlog, [&] { ERRLOG_CATCH(e); });
        EXPECT_EQ(concat_tostr(__FILE__, ':', logging_line,
                          ": Caught exception -> abc"
                          "\nStack unwinding marks:"
                          "\n[0] void foo(bool) at ",
                          __FILE__, ':', foo_mark_line, "\n[1] ", __PRETTY_FUNCTION__, " at ",
                          __FILE__, ':', mark_line, "\n"),
                logged_catch);
    } catch (...) {
        ADD_FAILURE();
    }
}

static void test_errlog_catch_and_stack_unwinding_mark_normal_no_args() {
    // Earlier stack unwinding marks should not appear in ERRLOG_CATCH()
    leave_stack_unwinding_mark();

    // Check discarding earlier unwinding marks and retaining of the recent ones
    constexpr auto mark_line = __LINE__ + 2;
    try {
        STACK_UNWINDING_MARK;
        foo(true);
        ADD_FAILURE();
    } catch (const std::exception& e) {
        EXPECT_EQ(string(foo_thrown_msg), e.what());
        constexpr auto logging_line = __LINE__ + 1;
        auto logged_catch = intercept_logger(errlog, [&] { ERRLOG_CATCH(); });
        EXPECT_EQ(concat_tostr(__FILE__, ':', logging_line,
                          ": Caught exception"
                          "\nStack unwinding marks:"
                          "\n[0] void foo(bool) at ",
                          __FILE__, ':', foo_mark_line, "\n[1] ", __PRETTY_FUNCTION__, " at ",
                          __FILE__, ':', mark_line, "\n"),
                logged_catch);
    } catch (...) {
        ADD_FAILURE();
    }
}

static void test_errlog_catch_and_stack_unwinding_mark_ignoring_older_marks() {
    // Earlier stack unwinding marks should not appear in ERRLOG_CATCH()
    leave_stack_unwinding_mark();

    // NOLINTNEXTLINE(readability-simplify-boolean-expr)
    if constexpr (/* DISABLES CODE */ (false)) {
        // Unfortunately, this test cannot be met as there is no way to get
        // information about current exception during stack unwinding
        try {
            throw std::exception();
        } catch (...) {
            constexpr auto logging_line = __LINE__ + 1;
            auto logged_catch = intercept_logger(errlog, [&] { ERRLOG_CATCH(); });
            EXPECT_EQ(concat_tostr(__FILE__, ':', logging_line,
                              ": Caught exception\n"
                              "Stack unwinding marks:\n"),
                    logged_catch);
        }
    }
}

static void test_errlog_catch_and_stack_unwinding_mark_ignoring_second_level() {
    // Second level stack unwinding marks should not be saved
    struct SecondLevel {
        SecondLevel() = default;

        SecondLevel(const SecondLevel&) = delete;
        SecondLevel(SecondLevel&&) = delete;
        SecondLevel& operator=(const SecondLevel&) = delete;
        SecondLevel& operator=(SecondLevel&&) = delete;

        ~SecondLevel() {
            try {
                foo(true);
                ADD_FAILURE();
            } catch (...) {
            }
        }
    };

    // Earlier stack unwinding marks should not appear in ERRLOG_CATCH()
    leave_stack_unwinding_mark();

    constexpr auto mark_line = __LINE__ + 2;
    try {
        STACK_UNWINDING_MARK;
        SecondLevel sec_lvl;
        foo(true);
        ADD_FAILURE();
    } catch (const std::exception& e) {
        EXPECT_EQ(string(foo_thrown_msg), e.what());
        constexpr auto logging_line = __LINE__ + 1;
        auto logged_catch = intercept_logger(errlog, [&] { ERRLOG_CATCH(e); });
        EXPECT_EQ(concat_tostr(__FILE__, ':', logging_line,
                          ": Caught exception -> abc"
                          "\nStack unwinding marks:"
                          "\n[0] void foo(bool) at ",
                          __FILE__, ':', foo_mark_line, "\n[1] ", __PRETTY_FUNCTION__, " at ",
                          __FILE__, ':', mark_line, "\n"),
                logged_catch);
    }
}

// NOLINTNEXTLINE
TEST(debug, ERRLOG_CATCH_AND_STACK_UNWINDING_MARK_MACROS) {
    STACK_UNWINDING_MARK;
    {
        STACK_UNWINDING_MARK;
    }

    EXPECT_NO_THROW(foo(false));

    test_errlog_catch_and_stack_unwinding_mark_ignoring_older_marks();
    test_errlog_catch_and_stack_unwinding_mark_normal();
    test_errlog_catch_and_stack_unwinding_mark_normal_no_args();
    test_errlog_catch_and_stack_unwinding_mark_ignoring_second_level();
}

// NOLINTNEXTLINE
TEST(debug_DeathTest, WONT_THROW_MACRO_fail) {
    EXPECT_EXIT(
            {
                errlog.label(false);
                std::vector<int> abc;
                errlog("BUG");
                (void)WONT_THROW(abc.at(42));
            },
            ::testing::KilledBySignal(SIGABRT), "^BUG\nBUG: this was expected to not throw\n$");
}

// NOLINTNEXTLINE
TEST(debug, WONT_THROW_MACRO_lvalue_reference) {
    {
        int a = 42;
        WONT_THROW(++a) = 42;
        EXPECT_EQ(a, 42);
    }
    {
        int a = 42;
        int& x = WONT_THROW(++a);
        ++a;
        EXPECT_EQ(x, 44);
    }
}

// NOLINTNEXTLINE
TEST(debug, WONT_THROW_MACRO_rvalue_reference) {
    auto ptr = std::make_unique<int>(162);
    std::unique_ptr<int> ptr2 = WONT_THROW(std::move(ptr));
    EXPECT_EQ((bool)ptr, false);
    EXPECT_EQ((bool)ptr2, true);
}

// NOLINTNEXTLINE
TEST(debug, WONT_THROW_MACRO_xvalue) {
    struct A {
        std::string& str;
        A(const A&) = delete;
        A(A&&) = delete;
        A& operator=(const A&) = delete;
        A& operator=(A&&) = delete;
        explicit A(std::string& s)
        : str(s) {
            str += "+A";
        }
        ~A() { str += "-A"; }
    };
    struct B {
        A a;
        B(const B&) = delete;
        B(B&&) = delete;
        B& operator=(const B&) = delete;
        B& operator=(B&&) = delete;
        ~B() = default;
        explicit B(std::string& str)
        : a(str) {}
        A&& get() && noexcept { return std::move(a); }
    };
    struct C {
        std::string& str;
        C(const C&) = delete;
        C(C&&) = delete;
        C& operator=(const C&) = delete;
        C& operator=(C&&) = delete;
        C(std::string& s, const A& /*unused*/)
        : str(s) {
            str += "+C";
        }
        ~C() { str += "-C"; }
    };

    std::string str;
    {
        C val(str, WONT_THROW(B(str).get()));
    }
    EXPECT_EQ(str, "+A+C-A-C");
}

// NOLINTNEXTLINE
TEST(debug, WONT_THROW_MACRO_prvalue) {
    std::unique_ptr<int> ptr = WONT_THROW(std::make_unique<int>(162));
    EXPECT_EQ((bool)ptr, true);
}

template <class T, class = decltype(T(WONT_THROW(T(42))))>
constexpr std::true_type double_construct_with_WONT_THROW_impl(int);

template <class>
constexpr std::false_type double_construct_with_WONT_THROW_impl(...);

template <class T>
constexpr bool double_construct_with_WONT_THROW =
        decltype(double_construct_with_WONT_THROW_impl<T>(0))::value;

// NOLINTNEXTLINE
TEST(debug, WONT_THROW_MACRO_prvalue_copy_elision) {
    struct X {
        explicit X(int /*unused*/) {}
        X(const X&) = delete;
        X(X&&) = delete;
        X& operator=(const X&) = delete;
        X& operator=(X&&) = delete;
        ~X() = default;
    };

    struct Y {
        explicit Y(int /*unused*/) {}
        Y(const Y&) = delete;
        Y(Y&&) = default;
        Y& operator=(const Y&) = delete;
        Y& operator=(Y&&) = default;
        ~Y() = default;
    };

    // Sadly WONT_THROW() does not work with prvalues
    static_assert(not double_construct_with_WONT_THROW<X>);
    // But works with x-values
    static_assert(double_construct_with_WONT_THROW<Y>);
}

// NOLINTNEXTLINE
TEST(debug, WONT_THROW_MACRO_rvalue) {
    int a = 4;
    int b = 7;
    int c = WONT_THROW(a + b);
    EXPECT_EQ(a, 4);
    EXPECT_EQ(b, 7);
    EXPECT_EQ(c, 11);
}

// NOLINTNEXTLINE
TEST(debug_DeathTest, WONT_THROW_MACRO_throw_in_the_same_statement_after_WONT_THROW_MACRO) {
    try {
        // NOLINTNEXTLINE(hicpp-exception-baseclass)
        auto factory = [] { return [] { throw 42; }; };
        WONT_THROW(factory())();
        FAIL() << "should have thrown";
    } catch (int x) { // NOLINT(cppcoreguidelines-init-variables)
        EXPECT_EQ(x, 42);
    }
}

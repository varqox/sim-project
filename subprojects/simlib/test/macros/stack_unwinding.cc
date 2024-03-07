#include "../intercept_logger.hh"

#include <gtest/gtest.h>
#include <simlib/concat_tostr.hh>
#include <simlib/macros/stack_unwinding.hh>

using std::string;

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
        EXPECT_EQ(
            concat_tostr(
                __FILE__,
                ':',
                logging_line,
                ": Caught exception -> abc"
                "\nStack unwinding marks:"
                "\n[0] void foo(bool) at ",
                __FILE__,
                ':',
                foo_mark_line,
                "\n[1] ",
                __PRETTY_FUNCTION__,
                " at ",
                __FILE__,
                ':',
                mark_line,
                "\n"
            ),
            logged_catch
        );
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
        EXPECT_EQ(
            concat_tostr(
                __FILE__,
                ':',
                logging_line,
                ": Caught exception"
                "\nStack unwinding marks:"
                "\n[0] void foo(bool) at ",
                __FILE__,
                ':',
                foo_mark_line,
                "\n[1] ",
                __PRETTY_FUNCTION__,
                " at ",
                __FILE__,
                ':',
                mark_line,
                "\n"
            ),
            logged_catch
        );
    } catch (...) {
        ADD_FAILURE();
    }
}

static void test_errlog_catch_and_stack_unwinding_mark_ignoring_older_marks() {
    // Earlier stack unwinding marks should not appear in ERRLOG_CATCH()
    leave_stack_unwinding_mark();

    // NOLINTNEXTLINE(readability-simplify-boolean-expr)
    if constexpr (/* DISABLES CODE */ false) {
        // Unfortunately, this test cannot be met as there is no way to get
        // information about current exception during stack unwinding
        try {
            throw std::exception();
        } catch (...) {
            constexpr auto logging_line = __LINE__ + 1;
            auto logged_catch = intercept_logger(errlog, [&] { ERRLOG_CATCH(); });
            EXPECT_EQ(
                concat_tostr(
                    __FILE__,
                    ':',
                    logging_line,
                    ": Caught exception\n"
                    "Stack unwinding marks:\n"
                ),
                logged_catch
            );
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
        EXPECT_EQ(
            concat_tostr(
                __FILE__,
                ':',
                logging_line,
                ": Caught exception -> abc"
                "\nStack unwinding marks:"
                "\n[0] void foo(bool) at ",
                __FILE__,
                ':',
                foo_mark_line,
                "\n[1] ",
                __PRETTY_FUNCTION__,
                " at ",
                __FILE__,
                ':',
                mark_line,
                "\n"
            ),
            logged_catch
        );
    }
}

// NOLINTNEXTLINE
TEST(macros, ERRLOG_CATCH_AND_STACK_UNWINDING_MARK_MACROS) {
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

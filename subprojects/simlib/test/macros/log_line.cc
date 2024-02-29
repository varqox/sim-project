#include "../intercept_logger.hh"

#include <gtest/gtest.h>
#include <simlib/macros/log_line.hh>

// NOLINTNEXTLINE
TEST(macros, LOG_LINE_MACRO) {
    auto logged_line = intercept_logger(stdlog, [] { LOG_LINE; });
    EXPECT_EQ(concat_tostr(__FILE__, ':', __LINE__ - 1, '\n'), logged_line);
    auto logged_line2 = intercept_logger(stdlog, [] { LOG_LINE; });
    EXPECT_EQ(concat_tostr(__FILE__, ':', __LINE__ - 1, '\n'), logged_line2);
}

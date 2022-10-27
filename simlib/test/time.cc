#include <chrono>
#include <gtest/gtest.h>
#include <simlib/time.hh>

using std::chrono_literals::operator""ns;
using std::chrono_literals::operator""ms;
using std::chrono_literals::operator""s;

// NOLINTNEXTLINE
TEST(DISABLED_time, microtime) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, date) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, date_with_curr_time) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, date_with_time_point) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, mysql_date) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, mysql_date_with_curr_time) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, mysql_date_with_time_point) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, localdate) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, localdate_with_curr_time) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, localdate_with_time_point) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, mysql_localdate) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, mysql_localdate_with_curr_time) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, mysql_localdate_with_time_point) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, is_datetime) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, str_to_time_t) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, str_to_time_point) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, timespec_operator_plus) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, timespec_operator_minus) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, timespec_operator_add) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, timespec_operator_equal) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, timespec_operator_not_equal) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, timespec_operator_less) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, timespec_operator_greater) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, timespec_operator_not_greater) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, timespec_operator_not_less) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, timeval_operator_plus) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, timeval_operator_minus) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, timeval_operator_add) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, timeval_operator_equal) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, timeval_operator_not_equal) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, timeval_operator_less) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, timeval_operator_greater) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, timeval_operator_not_greater) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, timeval_operator_not_less) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(time, timespec_to_string) {
    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 123456789}, 9, false) ==
            "1234567890.123456789");
    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 123456789}, 8, false) ==
            "1234567890.12345678");
    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 123456789}, 7, false) ==
            "1234567890.1234567");
    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 123456789}, 6, false) ==
            "1234567890.123456");
    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 123456789}, 5, false) ==
            "1234567890.12345");
    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 123456789}, 4, false) ==
            "1234567890.1234");
    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 123456789}, 3, false) ==
            "1234567890.123");
    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 123456789}, 2, false) ==
            "1234567890.12");
    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 123456789}, 1, false) ==
            "1234567890.1");
    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 123456789}, 0, false) ==
            "1234567890");

    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 123456789}, 9, true) ==
            "1234567890.123456789");
    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 123456789}, 8, true) ==
            "1234567890.12345678");
    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 123456789}, 7, true) ==
            "1234567890.1234567");
    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 123456789}, 6, true) ==
            "1234567890.123456");
    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 123456789}, 5, true) ==
            "1234567890.12345");
    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 123456789}, 4, true) ==
            "1234567890.1234");
    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 123456789}, 3, true) ==
            "1234567890.123");
    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 123456789}, 2, true) ==
            "1234567890.12");
    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 123456789}, 1, true) ==
            "1234567890.1");
    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 123456789}, 0, true) ==
            "1234567890");

    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 123456780}, 9, true) ==
            "1234567890.12345678");
    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 123456709}, 8, true) ==
            "1234567890.1234567");
    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 123456089}, 7, true) ==
            "1234567890.123456");
    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 123450789}, 6, true) ==
            "1234567890.12345");
    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 123406789}, 5, true) ==
            "1234567890.1234");
    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 123056789}, 4, true) ==
            "1234567890.123");
    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 120456789}, 3, true) ==
            "1234567890.12");
    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 103456789}, 2, true) ==
            "1234567890.1");
    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 23456789}, 1, true) ==
            "1234567890");

    static_assert(timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 100000000}, 9, true) ==
            "1234567890.1");
    static_assert(
            timespec_to_string({.tv_sec = 1234567890, .tv_nsec = 0}, 9, true) == "1234567890");
}

// NOLINTNEXTLINE
TEST(time, timeval_to_string) {
    static_assert(timeval_to_string({.tv_sec = 1234567890, .tv_usec = 123456}, 6, false) ==
            "1234567890.123456");
    static_assert(timeval_to_string({.tv_sec = 1234567890, .tv_usec = 123456}, 5, false) ==
            "1234567890.12345");
    static_assert(timeval_to_string({.tv_sec = 1234567890, .tv_usec = 123456}, 4, false) ==
            "1234567890.1234");
    static_assert(timeval_to_string({.tv_sec = 1234567890, .tv_usec = 123456}, 3, false) ==
            "1234567890.123");
    static_assert(timeval_to_string({.tv_sec = 1234567890, .tv_usec = 123456}, 2, false) ==
            "1234567890.12");
    static_assert(timeval_to_string({.tv_sec = 1234567890, .tv_usec = 123456}, 1, false) ==
            "1234567890.1");
    static_assert(
            timeval_to_string({.tv_sec = 1234567890, .tv_usec = 123456}, 0, false) == "1234567890");

    static_assert(timeval_to_string({.tv_sec = 1234567890, .tv_usec = 123456}, 6, true) ==
            "1234567890.123456");
    static_assert(timeval_to_string({.tv_sec = 1234567890, .tv_usec = 123456}, 5, true) ==
            "1234567890.12345");
    static_assert(timeval_to_string({.tv_sec = 1234567890, .tv_usec = 123456}, 4, true) ==
            "1234567890.1234");
    static_assert(timeval_to_string({.tv_sec = 1234567890, .tv_usec = 123456}, 3, true) ==
            "1234567890.123");
    static_assert(timeval_to_string({.tv_sec = 1234567890, .tv_usec = 123456}, 2, true) ==
            "1234567890.12");
    static_assert(timeval_to_string({.tv_sec = 1234567890, .tv_usec = 123456}, 1, true) ==
            "1234567890.1");
    static_assert(
            timeval_to_string({.tv_sec = 1234567890, .tv_usec = 123456}, 0, true) == "1234567890");

    static_assert(timeval_to_string({.tv_sec = 1234567890, .tv_usec = 123450}, 6, true) ==
            "1234567890.12345");
    static_assert(timeval_to_string({.tv_sec = 1234567890, .tv_usec = 123406}, 5, true) ==
            "1234567890.1234");
    static_assert(timeval_to_string({.tv_sec = 1234567890, .tv_usec = 123056}, 4, true) ==
            "1234567890.123");
    static_assert(timeval_to_string({.tv_sec = 1234567890, .tv_usec = 120456}, 3, true) ==
            "1234567890.12");
    static_assert(timeval_to_string({.tv_sec = 1234567890, .tv_usec = 103456}, 2, true) ==
            "1234567890.1");
    static_assert(
            timeval_to_string({.tv_sec = 1234567890, .tv_usec = 23456}, 1, true) == "1234567890");

    static_assert(timeval_to_string({.tv_sec = 1234567890, .tv_usec = 100000}, 6, true) ==
            "1234567890.1");
    static_assert(timeval_to_string({.tv_sec = 1234567890, .tv_usec = 0}, 6, true) == "1234567890");
}

// NOLINTNEXTLINE
TEST(DISABLED_time, is_power_of_10) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(time, to_string_with_duration) {
    static_assert(intentional_unsafe_string_view(to_string(1s)) == "1");
    static_assert(intentional_unsafe_string_view(to_string(1012300000ns)) == "1.0123");
    static_assert(intentional_unsafe_string_view(to_string(100123456780ns)) == "100.12345678");
    static_assert(intentional_unsafe_string_view(to_string(123123456789ns)) == "123.123456789");
    static_assert(intentional_unsafe_string_view(to_string(123456789ns)) == "0.123456789");
    static_assert(intentional_unsafe_string_view(to_string(89ns)) == "0.000000089");
    static_assert(intentional_unsafe_string_view(to_string(800ns)) == "0.0000008");
    static_assert(intentional_unsafe_string_view(to_string(12ms)) == "0.012");
    static_assert(intentional_unsafe_string_view(to_string(1230000ms)) == "1230");

    static_assert(intentional_unsafe_string_view(to_string(1012300000ns, false)) == "1.012300000");
    static_assert(intentional_unsafe_string_view(to_string(800ns, false)) == "0.000000800");
    static_assert(intentional_unsafe_string_view(to_string(12ms, false)) == "0.012");
    static_assert(intentional_unsafe_string_view(to_string(1230000ms, false)) == "1230.000");
}

// NOLINTNEXTLINE
TEST(DISABLED_time, to_duration_with_timeval) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, to_duration_with_timespec) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, to_timespec) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_time, to_nanoseconds) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(time, floor_to_10ms) {
    static_assert(floor_to_10ms(1s) == 1s);
    static_assert(floor_to_10ms(1012300000ns) == 1010ms);
    static_assert(floor_to_10ms(100123456780ns) == 100120ms);
    static_assert(floor_to_10ms(123123456789ns) == 123120ms);
    static_assert(floor_to_10ms(123456789ns) == 120ms);
    static_assert(floor_to_10ms(89ns) == 0s);
    static_assert(floor_to_10ms(800ns) == 0s);
    static_assert(floor_to_10ms(12ms) == 10ms);
    static_assert(floor_to_10ms(1230000ms) == 1230s);

    static_assert(floor_to_10ms(1230ms) == 1230ms);
    static_assert(floor_to_10ms(1231ms) == 1230ms);
    static_assert(floor_to_10ms(1232ms) == 1230ms);
    static_assert(floor_to_10ms(1233ms) == 1230ms);
    static_assert(floor_to_10ms(1234ms) == 1230ms);
    static_assert(floor_to_10ms(1235ms) == 1230ms);
    static_assert(floor_to_10ms(1236ms) == 1230ms);
    static_assert(floor_to_10ms(1237ms) == 1230ms);
    static_assert(floor_to_10ms(1238ms) == 1230ms);
    static_assert(floor_to_10ms(1239ms) == 1230ms);
}

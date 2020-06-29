#include "simlib/time.hh"

#include <chrono>
#include <gtest/gtest.h>

using std::chrono_literals::operator""ns;
using std::chrono_literals::operator""ms;
using std::chrono_literals::operator""s;

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, microtime) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, date) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, date_with_curr_time) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, date_with_time_point) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, mysql_date) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, mysql_date_with_curr_time) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, mysql_date_with_time_point) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, localdate) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, localdate_with_curr_time) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, localdate_with_time_point) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, mysql_localdate) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, mysql_localdate_with_curr_time) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, mysql_localdate_with_time_point) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, is_datetime) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, str_to_time_t) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, str_to_time_point) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, timespec_operator_plus) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, timespec_operator_minus) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, timespec_operator_add) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, timespec_operator_equal) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, timespec_operator_not_equal) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, timespec_operator_less) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, timespec_operator_greater) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, timespec_operator_not_greater) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, timespec_operator_not_less) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, timeval_operator_plus) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, timeval_operator_minus) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, timeval_operator_add) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, timeval_operator_equal) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, timeval_operator_not_equal) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, timeval_operator_less) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, timeval_operator_greater) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, timeval_operator_not_greater) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, timeval_operator_not_less) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, timespec_to_string) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, timeval_to_string) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, is_power_of_10) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(time, to_string_with_duration) {
	static_assert(intentional_unsafe_string_view(to_string(1s)) == "1");
	static_assert(intentional_unsafe_string_view(to_string(1012300000ns)) ==
	              "1.0123");
	static_assert(intentional_unsafe_string_view(to_string(100123456780ns)) ==
	              "100.12345678");
	static_assert(intentional_unsafe_string_view(to_string(123123456789ns)) ==
	              "123.123456789");
	static_assert(intentional_unsafe_string_view(to_string(123456789ns)) ==
	              "0.123456789");
	static_assert(intentional_unsafe_string_view(to_string(89ns)) ==
	              "0.000000089");
	static_assert(intentional_unsafe_string_view(to_string(800ns)) ==
	              "0.0000008");
	static_assert(intentional_unsafe_string_view(to_string(12ms)) == "0.012");
	static_assert(intentional_unsafe_string_view(to_string(1230000ms)) ==
	              "1230");

	static_assert(intentional_unsafe_string_view(
	                 to_string(1012300000ns, false)) == "1.012300000");
	static_assert(intentional_unsafe_string_view(to_string(800ns, false)) ==
	              "0.000000800");
	static_assert(intentional_unsafe_string_view(to_string(12ms, false)) ==
	              "0.012");
	static_assert(intentional_unsafe_string_view(to_string(1230000ms, false)) ==
	              "1230.000");
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, to_duration_with_timeval) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, to_duration_with_timespec) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, to_timespec) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(DISABLED_time, to_nanoseconds) {
	// TODO: implement it
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
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

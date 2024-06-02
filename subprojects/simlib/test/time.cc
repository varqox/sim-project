#include <chrono>
#include <cstdint>
#include <ctime>
#include <gtest/gtest.h>
#include <simlib/string_transform.hh>
#include <simlib/time.hh>
#include <thread>

namespace {

void check_datetime(
    const std::string& datetime_str,
    const std::chrono::system_clock::time_point& tp,
    time_t (*tm_to_time_t)(struct tm*)
) {
    EXPECT_TRUE(is_datetime(datetime_str.c_str()));

    auto tmt = tm{};
    tmt.tm_year = str2num<int32_t>(std::string_view{datetime_str.data(), 4}).value() - 1900;
    tmt.tm_mon = str2num<int32_t>(std::string_view{datetime_str.data() + 5, 2}).value() - 1;
    tmt.tm_mday = str2num<int32_t>(std::string_view{datetime_str.data() + 8, 2}).value();
    tmt.tm_hour = str2num<int32_t>(std::string_view{datetime_str.data() + 11, 2}).value();
    tmt.tm_min = str2num<int32_t>(std::string_view{datetime_str.data() + 14, 2}).value();
    tmt.tm_sec = str2num<int32_t>(std::string_view{datetime_str.data() + 17, 2}).value();
    tmt.tm_isdst = -1;

    auto t = tm_to_time_t(&tmt);
    ASSERT_NE(t, -1);
    auto tp_from_t = std::chrono::system_clock::time_point{std::chrono::seconds{t}};
    EXPECT_LE(tp_from_t, tp);
    EXPECT_LT(
        tp - tp_from_t, std::chrono::milliseconds(1100)
    ); // 100ms for delay, and 1000ms for cases where now == X.999 s -> datetime_str will be
       // rounded down to X s
}

} // namespace

// NOLINTNEXTLINE
TEST(time, utc_mysql_datetime_from_time_t) {
    EXPECT_EQ(utc_mysql_datetime_from_time_t(0), "1970-01-01 00:00:00");
    EXPECT_EQ(utc_mysql_datetime_from_time_t(1'717'343'210), "2024-06-02 15:46:50");
}

// NOLINTNEXTLINE
TEST(time, utc_mysql_datetime) {
    std::this_thread::yield(); // Maximize the probability that the next instructions are executed
                               // one after the other with minimal delay
    auto utc_curr_time = utc_mysql_datetime();
    auto tp = std::chrono::system_clock::now();

    check_datetime(utc_curr_time, tp, timegm);
}

// NOLINTNEXTLINE
TEST(time, utc_mysql_datetime_with_offset) {
    std::this_thread::yield(); // Maximize the probability that the next instructions are executed
                               // one after the other with minimal delay
    auto utc_curr_time_plus_10 = utc_mysql_datetime_with_offset(10);
    auto tp_plus_10 = std::chrono::system_clock::now() + std::chrono::seconds{10};

    check_datetime(utc_curr_time_plus_10, tp_plus_10, timegm);
}

// NOLINTNEXTLINE
TEST(time, local_mysql_datetime) {
    std::this_thread::yield(); // Maximize the probability that the next instructions are executed
                               // one after the other with minimal delay
    auto local_curr_time = local_mysql_datetime();
    auto tp = std::chrono::system_clock::now();

    check_datetime(local_curr_time, tp, mktime);
}

// NOLINTNEXTLINE
TEST(time, is_datetime) {
    EXPECT_EQ(is_datetime("2022-02-22 22:22:22"), true);
    EXPECT_EQ(is_datetime("2022-01-01 00:00:00"), true);
    EXPECT_EQ(is_datetime("2022-12-31 23:59:59"), true);

    EXPECT_EQ(is_datetime(""), false);
    EXPECT_EQ(is_datetime("2022-02-22 22:22:2"), false);
    EXPECT_EQ(is_datetime("2022-02-2222:22:22"), false);
    EXPECT_EQ(is_datetime("20222-02-22 22:22:22"), false);
    EXPECT_EQ(is_datetime("0000-02-22 22:22:22"), true);

    EXPECT_EQ(is_datetime("2022-00-22 22:22:22"), false);
    EXPECT_EQ(is_datetime("2022-13-22 22:22:22"), false);
    EXPECT_EQ(is_datetime("2022-02-00 22:22:22"), false);
    EXPECT_EQ(is_datetime("2022-01-32 22:22:22"), false);
    EXPECT_EQ(is_datetime("2022-02-22 24:22:22"), false);
    EXPECT_EQ(is_datetime("2022-02-22 22:60:22"), false);
    EXPECT_EQ(is_datetime("2022-02-22 22:22:62"), false);
}

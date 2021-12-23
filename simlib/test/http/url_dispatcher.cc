#include "simlib/http/url_dispatcher.hh"
#include "simlib/logger.hh"
#include "simlib/string_traits.hh"
#include "simlib/string_transform.hh"
#include "simlib/string_view.hh"

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

static constexpr std::optional<std::tuple<>> is_xyz(StringView str) {
    if (str == "xyz") {
        return std::tuple{};
    }
    return std::nullopt;
}

// NOLINTNEXTLINE
TEST(http, UrlParser) {
    using http::UrlParser;
    static constexpr char url[] = "/{i64}/abcxyz/{i64}/{string}/{u64}/{custom}";
    static_assert(UrlParser<url, is_xyz>::literal_prefix == "/");
    static_assert(UrlParser<url, is_xyz>::literal_suffix.empty());
    // Parsing
    static_assert(UrlParser<url, is_xyz>::try_parse("/-8/abcxyz/42/hohyha/7/xyz") ==
            std::tuple(-8, 42, "hohyha", 7, std::tuple()));
    static_assert(
            UrlParser<url, is_xyz>::try_parse("/-8/abcxyz/42/hohyha/-7/xyz") == std::nullopt);
    static_assert(std::is_same_v<typename UrlParser<url, is_xyz>::HandlerArgsTuple,
            std::tuple<int64_t, int64_t, StringView, uint64_t, std::tuple<>>>);
    // Prefixes and suffixes
    static constexpr char url1[] = "/a/bc/def/hijk/{i64}/abcxyz/{i64}/{string}/{u64}/{custom}";
    static_assert(UrlParser<url1, is_xyz>::literal_prefix == "/a/bc/def/hijk/");
    static_assert(UrlParser<url1, is_xyz>::literal_suffix.empty());
    static constexpr char url2[] =
            "/a/bc/def/hijk/{i64}/abcxyz/{i64}/{string}/{u64}/{custom}/";
    static_assert(UrlParser<url2, is_xyz>::literal_prefix == "/a/bc/def/hijk/");
    static_assert(UrlParser<url2, is_xyz>::literal_suffix == "/");
    static constexpr char url3[] =
            "/a/bc/def/hijk/{i64}/abcxyz/{i64}/{string}/{u64}/{custom}/x/yz/okl/";
    static_assert(UrlParser<url3, is_xyz>::literal_prefix == "/a/bc/def/hijk/");
    static_assert(UrlParser<url3, is_xyz>::literal_suffix == "/x/yz/okl/");
    static constexpr char url4[] =
            "/a/bc/def/hijk/{i64}/abcxyz/{i64}/{string}/{u64}/{custom}/x/yz/okl";
    static_assert(UrlParser<url4, is_xyz>::literal_prefix == "/a/bc/def/hijk/");
    static_assert(UrlParser<url4, is_xyz>::literal_suffix == "/x/yz/okl");
    static constexpr char url5[] = "//a/bc/def//hijk/{i64}/abcxyz/{i64}/"
                                   "{string}/{u64}/{custom}/x//yz/okl//";
    static_assert(UrlParser<url5, is_xyz>::literal_prefix == "//a/bc/def//hijk/");
    static_assert(UrlParser<url5, is_xyz>::literal_suffix == "/x//yz/okl//");
    static constexpr char url6[] = "/";
    static_assert(UrlParser<url6>::literal_prefix == "/");
    static_assert(UrlParser<url6>::literal_suffix == "/");
    static constexpr char url7[] = "/abc/xyz/hohoho";
    static_assert(UrlParser<url7>::literal_prefix == "/abc/xyz/hohoho");
    static_assert(UrlParser<url7>::literal_suffix == "/abc/xyz/hohoho");
    // Last argument is empty string
    static constexpr char url8[] = "/{i64}/x/{string}";
    static_assert(UrlParser<url8>::try_parse("/123/x/") == std::tuple(123, ""));
    static_assert(UrlParser<url8>::try_parse("/123/x/4a02bc") == std::tuple(123, "4a02bc"));
    static_assert(
            UrlParser<url8>::try_parse("/123/x") == std::nullopt); // Every '/' is required
}

// NOLINTNEXTLINE
TEST(http, UrlDispatcher_simple) {
    http::UrlDispatcher<int> ud;
    static constexpr char url[] = "/foo/{i64}/bar/{string}/test";
    int runs = 0;
    ud.add_handler<url>([&](int64_t a, StringView b) {
        ++runs;
        EXPECT_EQ(a, -42);
        EXPECT_EQ(b, "hohahyhu!xd");
        return 5;
    });
    EXPECT_EQ(ud.dispatch("/foo/-42/bar/hohahyhu!xd/test"), 5);
    EXPECT_EQ(runs, 1);
}

template <class ResponseT>
static auto canonized_collisions(const http::UrlDispatcher<ResponseT>& ud) {
    auto collisions = ud.all_potential_collisions();
    for (auto& [a, b] : collisions) {
        if (a > b) {
            std::swap(a, b);
        }
    }
    sort(collisions.begin(), collisions.end());
    return collisions;
}

// NOLINTNEXTLINE
TEST(http, UrlDispatcher_i8) {
    http::UrlDispatcher<int8_t> ud;
    static constexpr char url[] = "/{i8}";
    ud.add_handler<url>([&](int8_t x) { return x; });
    EXPECT_EQ(ud.dispatch("/-129"), std::nullopt);
    EXPECT_EQ(ud.dispatch("/-128"), -128);
    EXPECT_EQ(ud.dispatch("/-17"), -17);
    EXPECT_EQ(ud.dispatch("/42"), 42);
    EXPECT_EQ(ud.dispatch("/127"), 127);
    EXPECT_EQ(ud.dispatch("/128"), std::nullopt);
}

// NOLINTNEXTLINE
TEST(http, UrlDispatcher_i16) {
    http::UrlDispatcher<int16_t> ud;
    static constexpr char url[] = "/{i16}";
    ud.add_handler<url>([&](int16_t x) { return x; });
    EXPECT_EQ(ud.dispatch("/-32769"), std::nullopt);
    EXPECT_EQ(ud.dispatch("/-32768"), -32768);
    EXPECT_EQ(ud.dispatch("/-17"), -17);
    EXPECT_EQ(ud.dispatch("/42"), 42);
    EXPECT_EQ(ud.dispatch("/32767"), 32767);
    EXPECT_EQ(ud.dispatch("/32768"), std::nullopt);
}

// NOLINTNEXTLINE
TEST(http, UrlDispatcher_i32) {
    http::UrlDispatcher<int32_t> ud;
    static constexpr char url[] = "/{i32}";
    ud.add_handler<url>([&](int32_t x) { return x; });
    EXPECT_EQ(ud.dispatch("/-2147483649"), std::nullopt);
    EXPECT_EQ(ud.dispatch("/-2147483648"), -2147483648);
    EXPECT_EQ(ud.dispatch("/-17"), -17);
    EXPECT_EQ(ud.dispatch("/42"), 42);
    EXPECT_EQ(ud.dispatch("/2147483647"), 2147483647);
    EXPECT_EQ(ud.dispatch("/2147483648"), std::nullopt);
}

// NOLINTNEXTLINE
TEST(http, UrlDispatcher_i64) {
    http::UrlDispatcher<int64_t> ud;
    static constexpr char url[] = "/{i64}";
    ud.add_handler<url>([&](int64_t x) { return x; });
    EXPECT_EQ(ud.dispatch("/-9223372036854775809"), std::nullopt);
    EXPECT_EQ(ud.dispatch("/-9223372036854775808"), -9223372036854775807 - 1);
    EXPECT_EQ(ud.dispatch("/-17"), -17);
    EXPECT_EQ(ud.dispatch("/42"), 42);
    EXPECT_EQ(ud.dispatch("/9223372036854775807"), 9223372036854775807);
    EXPECT_EQ(ud.dispatch("/9223372036854775808"), std::nullopt);
}

// NOLINTNEXTLINE
TEST(http, UrlDispatcher_u8) {
    http::UrlDispatcher<uint8_t> ud;
    static constexpr char url[] = "/{u8}";
    ud.add_handler<url>([&](uint8_t x) { return x; });
    EXPECT_EQ(ud.dispatch("/-1"), std::nullopt);
    EXPECT_EQ(ud.dispatch("/0"), 0);
    EXPECT_EQ(ud.dispatch("/1"), 1);
    EXPECT_EQ(ud.dispatch("/42"), 42);
    EXPECT_EQ(ud.dispatch("/255"), 255);
    EXPECT_EQ(ud.dispatch("/256"), std::nullopt);
}

// NOLINTNEXTLINE
TEST(http, UrlDispatcher_u16) {
    http::UrlDispatcher<uint16_t> ud;
    static constexpr char url[] = "/{u16}";
    ud.add_handler<url>([&](uint16_t x) { return x; });
    EXPECT_EQ(ud.dispatch("/-1"), std::nullopt);
    EXPECT_EQ(ud.dispatch("/0"), 0);
    EXPECT_EQ(ud.dispatch("/1"), 1);
    EXPECT_EQ(ud.dispatch("/42"), 42);
    EXPECT_EQ(ud.dispatch("/65535"), 65535);
    EXPECT_EQ(ud.dispatch("/65536"), std::nullopt);
}

// NOLINTNEXTLINE
TEST(http, UrlDispatcher_u32) {
    http::UrlDispatcher<uint32_t> ud;
    static constexpr char url[] = "/{u32}";
    ud.add_handler<url>([&](uint32_t x) { return x; });
    EXPECT_EQ(ud.dispatch("/-1"), std::nullopt);
    EXPECT_EQ(ud.dispatch("/0"), 0);
    EXPECT_EQ(ud.dispatch("/1"), 1);
    EXPECT_EQ(ud.dispatch("/42"), 42);
    EXPECT_EQ(ud.dispatch("/4294967295"), 4294967295);
    EXPECT_EQ(ud.dispatch("/4294967296"), std::nullopt);
}

// NOLINTNEXTLINE
TEST(http, UrlDispatcher_u64) {
    http::UrlDispatcher<uint64_t> ud;
    static constexpr char url[] = "/{u64}";
    ud.add_handler<url>([&](uint64_t x) { return x; });
    EXPECT_EQ(ud.dispatch("/-1"), std::nullopt);
    EXPECT_EQ(ud.dispatch("/0"), 0);
    EXPECT_EQ(ud.dispatch("/1"), 1);
    EXPECT_EQ(ud.dispatch("/42"), 42);
    EXPECT_EQ(ud.dispatch("/18446744073709551615"), 18446744073709551615U);
    EXPECT_EQ(ud.dispatch("/18446744073709551616"), std::nullopt);
}

// NOLINTNEXTLINE
TEST(http, UrlDispatcher_overlapping_patterns) {
    http::UrlDispatcher<int> ud;
    static constexpr char url0[] = "/{i64}/{u64}/{u64}/";
    static constexpr char url1[] = "/{u64}/{i64}/{u64}/";
    static constexpr char url2[] = "/{u64}/{u64}/{i64}/";
    int runs = 0;
    ud.add_handler<url0>([&](int64_t a, uint64_t b, uint64_t c) {
        ++runs;
        EXPECT_EQ(a, -1);
        EXPECT_EQ(b, 2);
        EXPECT_EQ(c, 3);
        return 5;
    });
    ud.add_handler<url1>([&](uint64_t a, int64_t b, uint64_t c) {
        ++runs;
        EXPECT_EQ(a, 1);
        EXPECT_EQ(b, -2);
        EXPECT_EQ(c, 3);
        return 55;
    });
    ud.add_handler<url2>([&](uint64_t a, uint64_t b, int64_t c) {
        ++runs;
        EXPECT_EQ(a, 1);
        EXPECT_EQ(b, 2);
        EXPECT_EQ(c, -3);
        return 555;
    });
    // Positive
    EXPECT_EQ(ud.dispatch("/-1/2/3/"), 5);
    EXPECT_EQ(ud.dispatch("/1/-2/3/"), 55);
    EXPECT_EQ(ud.dispatch("/1/2/-3/"), 555);
    EXPECT_EQ(runs, 3);
    // Negative
    EXPECT_EQ(ud.dispatch("/1/-2/-3/"), std::nullopt);
    EXPECT_EQ(ud.dispatch("/-1/2/-3/"), std::nullopt);
    EXPECT_EQ(ud.dispatch("/-1/-2/3/"), std::nullopt);
    EXPECT_EQ(runs, 3);

    auto collisions = canonized_collisions(ud);
    EXPECT_EQ(collisions, (decltype(collisions){{url0, url1}, {url0, url2}, {url1, url2}}));
}

// NOLINTNEXTLINE
TEST(http, UrlDispatcher_all_potential_collisions) {
    http::UrlDispatcher<int> ud;
    using VC = decltype(ud.all_potential_collisions());

    static constexpr char url0[] = "/a/b/c/{string}/x/y/z/";
    ud.add_handler<url0>([&](StringView /*unused*/) { return 0; });
    EXPECT_EQ(canonized_collisions(ud), VC{});

    static constexpr char url1[] = "/a/b/{string}/o/{string}/y/z/";
    ud.add_handler<url1>([&](StringView /*unused*/, StringView /*unused*/) { return 0; });

    static constexpr char url2[] = "/a/b/c/o/{string}/y/z/";
    ud.add_handler<url2>([&](StringView /*unused*/) { return 0; });
    EXPECT_EQ(canonized_collisions(ud), (VC{{url2, url0}, {url2, url1}, {url0, url1}}));

    static constexpr char url3[] = "/a/b/{string}/o/x/y/z/";
    ud.add_handler<url3>([&](StringView /*unused*/) { return 0; });
    EXPECT_EQ(canonized_collisions(ud),
            (VC{{url2, url0}, {url2, url3}, {url2, url1}, {url0, url3}, {url0, url1},
                    {url3, url1}}));
    // This url does not collide
    static constexpr char url4[] = "/z/y/x/{string}/c/b/a/";
    ud.add_handler<url4>([&](StringView /*unused*/) { return 0; });
    EXPECT_EQ(canonized_collisions(ud),
            (VC{{url2, url0}, {url2, url3}, {url2, url1}, {url0, url3}, {url0, url1},
                    {url3, url1}}));
}

// NOLINTNEXTLINE
TEST(http, UrlDispatcher_two_handlers_for_the_same_url_without_custom_prasers) {
    http::UrlDispatcher<int> ud;
    using VC = decltype(ud.all_potential_collisions());

    static constexpr char url[] = "/a/{i64}/b";
    ud.add_handler<url>([&](int64_t /*unused*/) { return 0; });
    ud.add_handler<url>([&](int64_t /*unused*/) { return 0; });

    EXPECT_EQ(canonized_collisions(ud), (VC{{url, url}}));
}

static std::optional<StringView> has_even_length(StringView s) {
    return s.size() % 2 == 0 ? std::optional{s} : std::nullopt;
}
static std::optional<StringView> has_odd_length(StringView s) {
    return s.size() % 2 == 1 ? std::optional{s} : std::nullopt;
}

// NOLINTNEXTLINE
TEST(http, UrlDispatcher_two_handlers_for_the_same_url_with_custom_prasers) {
    http::UrlDispatcher<int> ud;
    using VC = decltype(ud.all_potential_collisions());

    static constexpr char url[] = "/a/{custom}/b";
    int runs = 0;
    ud.add_handler<url, has_even_length>([&](StringView s) {
        ++runs;
        EXPECT_EQ(s.size() % 2, 0);
        return 0;
    });
    ud.add_handler<url, &has_odd_length>([&](StringView s) {
        ++runs;
        EXPECT_EQ(s.size() % 2, 1);
        return 1;
    });

    EXPECT_EQ(canonized_collisions(ud), (VC{{url, url}}));

    EXPECT_EQ(ud.dispatch("/a/x/b"), 1);
    EXPECT_EQ(ud.dispatch("/a/xy/b"), 0);
    EXPECT_EQ(ud.dispatch("/a/xyz/b"), 1);
    EXPECT_EQ(ud.dispatch("/a/xyza/b"), 0);
    EXPECT_EQ(runs, 4);
}

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

using std::array;

static constexpr std::optional<std::tuple<>> is_xyz(StringView str) {
    if (str == "xyz") {
        return std::tuple{};
    }
    return std::nullopt;
}

// NOLINTNEXTLINE
TEST(http, UrlParser) {
    using http::UrlParser;
    static constexpr const char url[] = "/{int}/abcxyz/{int}/{string}/{uint}/{custom}";
    static_assert(UrlParser<url, is_xyz>::literal_prefix == "/");
    static_assert(UrlParser<url, is_xyz>::literal_suffix == "");
    // Parsing
    static_assert(
        UrlParser<url, is_xyz>::try_parse("/-8/abcxyz/42/hohyha/7/xyz") ==
        std::tuple(-8, 42, "hohyha", 7, std::tuple()));
    static_assert(
        UrlParser<url, is_xyz>::try_parse("/-8/abcxyz/42/hohyha/-7/xyz") == std::nullopt);
    static_assert(std::is_same_v<
                  typename UrlParser<url, is_xyz>::HandlerArgsTuple,
                  std::tuple<int64_t, int64_t, StringView, uint64_t, std::tuple<>>>);
    // Prefixes and suffixes
    static constexpr const char url1[] =
        "/a/bc/def/hijk/{int}/abcxyz/{int}/{string}/{uint}/{custom}";
    static_assert(UrlParser<url1, is_xyz>::literal_prefix == "/a/bc/def/hijk/");
    static_assert(UrlParser<url1, is_xyz>::literal_suffix == "");
    static constexpr const char url2[] =
        "/a/bc/def/hijk/{int}/abcxyz/{int}/{string}/{uint}/{custom}/";
    static_assert(UrlParser<url2, is_xyz>::literal_prefix == "/a/bc/def/hijk/");
    static_assert(UrlParser<url2, is_xyz>::literal_suffix == "/");
    static constexpr const char url3[] =
        "/a/bc/def/hijk/{int}/abcxyz/{int}/{string}/{uint}/{custom}/x/yz/okl/";
    static_assert(UrlParser<url3, is_xyz>::literal_prefix == "/a/bc/def/hijk/");
    static_assert(UrlParser<url3, is_xyz>::literal_suffix == "/x/yz/okl/");
    static constexpr const char url4[] =
        "/a/bc/def/hijk/{int}/abcxyz/{int}/{string}/{uint}/{custom}/x/yz/okl";
    static_assert(UrlParser<url4, is_xyz>::literal_prefix == "/a/bc/def/hijk/");
    static_assert(UrlParser<url4, is_xyz>::literal_suffix == "/x/yz/okl");
    static constexpr const char url5[] = "//a/bc/def//hijk/{int}/abcxyz/{int}/"
                                         "{string}/{uint}/{custom}/x//yz/okl//";
    static_assert(UrlParser<url5, is_xyz>::literal_prefix == "//a/bc/def//hijk/");
    static_assert(UrlParser<url5, is_xyz>::literal_suffix == "/x//yz/okl//");
    static constexpr const char url6[] = "/";
    static_assert(UrlParser<url6>::literal_prefix == "/");
    static_assert(UrlParser<url6>::literal_suffix == "/");
    static constexpr const char url7[] = "/abc/xyz/hohoho";
    static_assert(UrlParser<url7>::literal_prefix == "/abc/xyz/hohoho");
    static_assert(UrlParser<url7>::literal_suffix == "/abc/xyz/hohoho");
    // Last argument is empty string
    static constexpr const char url8[] = "/{int}/x/{string}";
    static_assert(UrlParser<url8>::try_parse("/123/x/") == std::tuple(123, ""));
    static_assert(UrlParser<url8>::try_parse("/123/x/4a02bc") == std::tuple(123, "4a02bc"));
    static_assert(
        UrlParser<url8>::try_parse("/123/x") == std::nullopt); // Every '/' is required
}

// NOLINTNEXTLINE
TEST(http, UrlDispatcher_simple) {
    http::UrlDispatcher ud;
    static constexpr const char url[] = "/foo/{int}/bar/{string}/test";
    int runs = 0;
    ud.add_handler<url>([&](int64_t a, StringView b) {
        ++runs;
        EXPECT_EQ(a, -42);
        EXPECT_EQ(b, "hohahyhu!xd");
        return http::Response{};
    });
    EXPECT_NE(ud.dispatch("/foo/-42/bar/hohahyhu!xd/test"), std::nullopt);
    EXPECT_EQ(runs, 1);
}

static auto canonized_collisions(const http::UrlDispatcher& ud) {
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
TEST(http, UrlDispatcher_overlapping_patterns) {
    http::UrlDispatcher ud;
    static constexpr const char url0[] = "/{int}/{uint}/{uint}/";
    static constexpr const char url1[] = "/{uint}/{int}/{uint}/";
    static constexpr const char url2[] = "/{uint}/{uint}/{int}/";
    array<int, 3> runs{};
    ud.add_handler<url0>([&](int64_t a, uint64_t b, uint64_t c) {
        ++runs[0];
        EXPECT_EQ(a, -1);
        EXPECT_EQ(b, 2);
        EXPECT_EQ(c, 3);
        return http::Response{};
    });
    ud.add_handler<url1>([&](uint64_t a, int64_t b, uint64_t c) {
        ++runs[1];
        EXPECT_EQ(a, 1);
        EXPECT_EQ(b, -2);
        EXPECT_EQ(c, 3);
        return http::Response{};
    });
    ud.add_handler<url2>([&](uint64_t a, uint64_t b, int64_t c) {
        ++runs[2];
        EXPECT_EQ(a, 1);
        EXPECT_EQ(b, 2);
        EXPECT_EQ(c, -3);
        return http::Response{};
    });
    // Positive
    EXPECT_NE(ud.dispatch("/-1/2/3/"), std::nullopt);
    EXPECT_NE(ud.dispatch("/1/-2/3/"), std::nullopt);
    EXPECT_NE(ud.dispatch("/1/2/-3/"), std::nullopt);
    EXPECT_EQ(runs, (array{1, 1, 1}));
    // Negative
    EXPECT_EQ(ud.dispatch("/1/-2/-3/"), std::nullopt);
    EXPECT_EQ(ud.dispatch("/-1/2/-3/"), std::nullopt);
    EXPECT_EQ(ud.dispatch("/-1/-2/3/"), std::nullopt);
    EXPECT_EQ(runs, (array{1, 1, 1}));

    auto collisions = canonized_collisions(ud);
    EXPECT_EQ(collisions, (decltype(collisions){{url0, url1}, {url0, url2}, {url1, url2}}));
}

// NOLINTNEXTLINE
TEST(http, UrlDispatcher_all_potential_collisions) {
    http::UrlDispatcher ud;
    using VC = decltype(ud.all_potential_collisions());

    static constexpr const char url0[] = "/a/b/c/{string}/x/y/z/";
    ud.add_handler<url0>([&](StringView /*unused*/) { return http::Response{}; });
    EXPECT_EQ(canonized_collisions(ud), VC{});

    static constexpr const char url1[] = "/a/b/{string}/o/{string}/y/z/";
    ud.add_handler<url1>(
        [&](StringView /*unused*/, StringView /*unused*/) { return http::Response{}; });

    static constexpr const char url2[] = "/a/b/c/o/{string}/y/z/";
    ud.add_handler<url2>([&](StringView /*unused*/) { return http::Response{}; });
    EXPECT_EQ(canonized_collisions(ud), (VC{{url2, url0}, {url2, url1}, {url0, url1}}));

    static constexpr const char url3[] = "/a/b/{string}/o/x/y/z/";
    ud.add_handler<url3>([&](StringView /*unused*/) { return http::Response{}; });
    EXPECT_EQ(
        canonized_collisions(ud),
        (VC{{url2, url0},
            {url2, url3},
            {url2, url1},
            {url0, url3},
            {url0, url1},
            {url3, url1}}));
    // This url does not collide
    static constexpr const char url4[] = "/z/y/x/{string}/c/b/a/";
    ud.add_handler<url4>([&](StringView /*unused*/) { return http::Response{}; });
    EXPECT_EQ(
        canonized_collisions(ud),
        (VC{{url2, url0},
            {url2, url3},
            {url2, url1},
            {url0, url3},
            {url0, url1},
            {url3, url1}}));
}

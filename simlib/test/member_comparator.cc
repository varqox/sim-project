#include "simlib/member_comparator.hh"
#include "simlib/string_view.hh"

#include <gtest/gtest.h>
#include <utility>
#include <vector>

using std::vector;

struct Foo {
    int x;
    StringView str;

    friend bool operator==(const Foo& a, const Foo& b) noexcept {
        return a.x == b.x and a.str == b.str;
    }
};

// NOLINTNEXTLINE
TEST(member_comparator, MEMBER_COMPARATOR_order) {
    vector<Foo> v = {
            {4, "d"},
            {2, "c"},
            {1, "a"},
            {3, "b"},
    };

    std::sort(v.begin(), v.end(), MEMBER_COMPARATOR(Foo, x){});
    EXPECT_EQ(v,
            (decltype(v){
                    {1, "a"},
                    {2, "c"},
                    {3, "b"},
                    {4, "d"},
            }));

    std::sort(v.begin(), v.end(), MEMBER_COMPARATOR(Foo, str){});
    EXPECT_EQ(v,
            (decltype(v){
                    {1, "a"},
                    {3, "b"},
                    {2, "c"},
                    {4, "d"},
            }));
}

// NOLINTNEXTLINE
TEST(member_comparator, TRANSPARENT_MEMBER_COMPARATOR_order) {
    vector<Foo> v = {
            {4, "d"},
            {2, "c"},
            {1, "a"},
            {3, "b"},
    };

    std::sort(v.begin(), v.end(), TRANSPARENT_MEMBER_COMPARATOR(Foo, x){});
    EXPECT_EQ(v,
            (decltype(v){
                    {1, "a"},
                    {2, "c"},
                    {3, "b"},
                    {4, "d"},
            }));

    std::sort(v.begin(), v.end(), TRANSPARENT_MEMBER_COMPARATOR(Foo, str){});
    EXPECT_EQ(v,
            (decltype(v){
                    {1, "a"},
                    {3, "b"},
                    {2, "c"},
                    {4, "d"},
            }));
}

// NOLINTNEXTLINE
TEST(member_comparator, TRANSPARENT_MEMBER_COMPARATOR_transparency) {
    TRANSPARENT_MEMBER_COMPARATOR(Foo, x) cmp{};
    EXPECT_TRUE((std::is_same_v<std::void_t<decltype(cmp)::is_transparent>, void>));

    EXPECT_EQ(cmp(Foo{0, "abc"}, 0), false);
    EXPECT_EQ(cmp(Foo{0, "abc"}, 1), true);
    EXPECT_EQ(cmp(Foo{0, "abc"}, 2), true);

    EXPECT_EQ(cmp(Foo{1, "abc"}, 0), false);
    EXPECT_EQ(cmp(Foo{1, "abc"}, 1), false);
    EXPECT_EQ(cmp(Foo{1, "abc"}, 2), true);

    EXPECT_EQ(cmp(Foo{2, "abc"}, 0), false);
    EXPECT_EQ(cmp(Foo{2, "abc"}, 1), false);
    EXPECT_EQ(cmp(Foo{2, "abc"}, 2), false);
}

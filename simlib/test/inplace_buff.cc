#include "simlib/inplace_buff.hh"
#include "simlib/random.hh"
#include "simlib/string_view.hh"

#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <utility>

using std::optional;
using std::string;

string random_str(size_t size) {
    string str(size, '\0');
    for (size_t i = 0; i < size; ++i) {
        str[i] =
            get_random(std::numeric_limits<char>::min(), std::numeric_limits<char>::max());
    }
    return str;
}

template <size_t N>
void random_check(InplaceBuff<N>& ibuff, string ibuff_as_str) {
    auto old_size = ibuff_as_str.size();
    switch (get_random(1, 2)) {
    case 1: {
        auto new_size = get_random<size_t>(0, old_size * 3);
        ibuff.resize(new_size);
        EXPECT_EQ(ibuff.size, new_size);
        if (new_size <= old_size) {
            EXPECT_EQ(StringView{ibuff}, StringView{ibuff_as_str}.substring(0, new_size));
        } else {
            EXPECT_EQ(StringView{ibuff}.substring(0, old_size), StringView{ibuff_as_str});
        }
    } break;
    case 2: {
        auto str = random_str(get_random<size_t>(0, old_size * 2));
        ibuff_as_str += str;
        ibuff.append(str);
        EXPECT_EQ(StringView{ibuff}, StringView{ibuff_as_str});
    } break;
    default: std::abort();
    }
}

template <size_t N, size_t OTHER_N>
void test_constructors(size_t size) {
    {
        // Default constructor
        InplaceBuff<N> ibuff;
        EXPECT_EQ(StringView{ibuff}, "");
        random_check(ibuff, "");
    }
    {
        // Constructor with given size
        InplaceBuff<N> ibuff(size);
        auto str = random_str(size);
        for (size_t i = 0; i < size; ++i) {
            ibuff[i] = str[i];
        }
        EXPECT_EQ(StringView{ibuff}, StringView{str});
        random_check(ibuff, std::move(str));
    }
    {
        // Copy construct from other InplaceBuff
        auto str = random_str(size);
        optional other = InplaceBuff<OTHER_N>{string{str}}; // use string copy
        auto ibuff = InplaceBuff<N>{*other};
        EXPECT_EQ(StringView{*other}, StringView{str});
        other = "";
        EXPECT_EQ(StringView{*other}, "");
        other.reset();
        EXPECT_EQ(StringView{ibuff}, StringView{str});
        random_check(ibuff, std::move(str));
    }
    {
        // Move construct from other InplaceBuff
        auto str = random_str(size);
        optional other = InplaceBuff<OTHER_N>{string{str}}; // use string copy
        auto ibuff = InplaceBuff<N>{std::move(*other)};
        other.reset();
        EXPECT_EQ(StringView{ibuff}, StringView{str});
        random_check(ibuff, std::move(str));
    }
    {
        // Copy construct from string
        auto str = random_str(size);
        auto ibuff = InplaceBuff<N>{string{str}}; // use string copy
        EXPECT_EQ(StringView{ibuff}, StringView{str});
        random_check(ibuff, std::move(str));
    }
    {
        // Inplace construct from string
        auto str = random_str(size);
        auto ibuff = InplaceBuff<N>{std::in_place, string{str}}; // use string copy
        EXPECT_EQ(StringView{ibuff}, StringView{str});
        random_check(ibuff, std::move(str));
    }
    {
        // Inplace construct from string two times
        auto str1 = random_str(size);
        auto str2 = random_str(size);
        auto ibuff =
            InplaceBuff<N>{std::in_place, string{str1}, string{str2}}; // use string copies
        auto str = str1 + str2;
        EXPECT_EQ(StringView{ibuff}, StringView{str});
        random_check(ibuff, std::move(str));
    }
    {
        // Construct from string two times
        auto str1 = random_str(size);
        auto str2 = random_str(size);
        auto ibuff = InplaceBuff<N>{string{str1}, string{str2}}; // use string copies
        auto str = str1 + str2;
        EXPECT_EQ(StringView{ibuff}, StringView{str});
        random_check(ibuff, std::move(str));
    }
    {
        // Construct from string two times
        auto str1 = random_str(size);
        auto str2 = random_str(size);
        auto str3 = random_str(size);
        auto ibuff =
            InplaceBuff<N>{string{str1}, string{str2}, string{str3}}; // use string copies
        auto str = str1 + str2 + str3;
        EXPECT_EQ(StringView{ibuff}, StringView{str});
        random_check(ibuff, std::move(str));
    }
}

template <size_t N, size_t OTHER_N>
void test_assignments(size_t size, size_t other_size) {
    {
        // Copy assign from other InplaceBuff
        auto ibuff = InplaceBuff<N>(random_str(size));
        auto str = random_str(other_size);
        optional other = InplaceBuff<OTHER_N>{string{str}}; // use string copy
        ibuff = *other;
        EXPECT_EQ(StringView{*other}, StringView{str});
        other = "";
        EXPECT_EQ(StringView{*other}, "");
        other.reset();
        EXPECT_EQ(StringView{ibuff}, StringView{str});
        random_check(ibuff, std::move(str));
    }
    {
        // Move assign from other InplaceBuff
        auto ibuff = InplaceBuff<N>(random_str(size));
        auto str = random_str(other_size);
        optional other = InplaceBuff<OTHER_N>{string{str}}; // use string copy
        ibuff = std::move(*other);
        other.reset();
        EXPECT_EQ(StringView{ibuff}, StringView{str});
        random_check(ibuff, std::move(str));
    }
    {
        // Copy assign from string
        auto ibuff = InplaceBuff<N>(random_str(size));
        auto str = random_str(other_size);
        ibuff = string{str}; // use string copy
        EXPECT_EQ(StringView{ibuff}, StringView{str});
        random_check(ibuff, std::move(str));
    }
}

template <size_t max_n, size_t max_other_n, size_t curr_n = 0, size_t curr_other_n = 0>
void test_constructors_and_assignments(size_t max_size, size_t max_other_size) {
    for (size_t size = 0; size < max_size; ++size) {
        test_constructors<curr_n, curr_other_n>(size);
        for (size_t other_size = 0; other_size < max_other_size; ++other_size) {
            test_assignments<curr_n, curr_other_n>(size, other_size);
        }
    }
    if constexpr (curr_other_n + 1 < max_other_n) {
        return test_constructors_and_assignments<max_n, max_other_n, curr_n, curr_other_n + 1>(
            max_size, max_other_size);
    } else if constexpr (curr_n + 1 < max_n) {
        return test_constructors_and_assignments<max_n, max_other_n, curr_n + 1, 0>(
            max_size, max_other_size);
    }
}

// NOLINTNEXTLINE
TEST(InplaceBuff, constructors_and_assignments) {
    test_constructors_and_assignments<11, 11>(11, 11);
}

// NOLINTNEXTLINE
TEST(DISABLED_InplaceBuff, size) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_InplaceBuff, lossy_resize) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_InplaceBuff, clear) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_InplaceBuff, begin) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_InplaceBuff, end) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_InplaceBuff, cbegin) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_InplaceBuff, cend) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_InplaceBuff, data) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_InplaceBuff, max_size) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_InplaceBuff, front) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_InplaceBuff, back) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_InplaceBuff, operator_subscript) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_InplaceBuff, to_string) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_InplaceBuff, to_cstr) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_InplaceBuff, append) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_inplace_buff, string_operator_add_inplace_buff) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_inplace_buff, intentional_unsafe_string_view) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_inplace_buff, intentional_unsafe_cstring_view) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_inplace_buff, string_length_of_inplace_buff) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_inplace_buff, back_insert) {
    // TODO: implement it
}

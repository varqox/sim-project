#include "simlib/file_path.hh"
#include "simlib/inplace_buff.hh"

#include <gtest/gtest.h>

using std::string;

// StringView may point to non-null-terminated string
static_assert(not std::is_convertible_v<FilePath, StringView&>);

// NOLINTNEXTLINE
TEST(FilePath, constructor_from_const_char_ptr) {
    auto test = [](auto&& arg) {
        const string& str(arg);
        FilePath x = arg;
        EXPECT_EQ(x.size(), str.size());
        EXPECT_EQ(x.data(), str);
    };

    test("abc");
    test("");
    test("e");
}

// NOLINTNEXTLINE
TEST(FilePath, constructor_from_const_cstring_view) {
    auto test = [](auto&& arg) {
        const string& str(arg);
        FilePath x = CStringView(arg);
        EXPECT_EQ(x.size(), str.size());
        EXPECT_EQ(x.data(), str);
    };

    test("abc");
    test("");
    test("e");
}

// NOLINTNEXTLINE
TEST(FilePath, constructor_from_string) {
    auto test = [](auto&& arg) {
        string str(arg);
        FilePath x = str;
        EXPECT_EQ(x.size(), str.size());
        EXPECT_EQ(x.data(), str);

        [&](FilePath y) {
            EXPECT_EQ(y.size(), str.size());
            EXPECT_EQ(y.data(), str);
        }(string(arg));
    };

    test("abc");
    test("");
    test("e");
}

// NOLINTNEXTLINE
TEST(FilePath, constructor_from_inplace_buff) {
    auto test = [](auto&& arg) {
        string str(arg);
        InplaceBuff<16> ib(arg);
        FilePath x = ib;
        EXPECT_EQ(x.size(), str.size());
        EXPECT_EQ(x.data(), str);

        [&](FilePath y) {
            EXPECT_EQ(y.size(), str.size());
            EXPECT_EQ(y.data(), str);
        }(InplaceBuff<16>(arg));
    };

    test("abc");
    test("");
    test("e");
}

static_assert(std::is_assignable_v<FilePath, FilePath&>);
static_assert(not std::is_assignable_v<FilePath, FilePath>);
static_assert(not std::is_assignable_v<FilePath, FilePath&&>);
static_assert(not std::is_assignable_v<FilePath, const FilePath&>);
static_assert(not std::is_assignable_v<FilePath, const FilePath&&>);

// NOLINTNEXTLINE
TEST(FilePath, assignment_from_const_char_ptr) {
    auto test = [](auto&& arg) {
        string str(arg);
        FilePath x = "";
        x = arg;
        EXPECT_EQ(x.size(), str.size());
        EXPECT_EQ(x.data(), str);
    };

    test("abc");
    test("");
    test("e");
}

// NOLINTNEXTLINE
TEST(FilePath, assignment_from_const_cstring_view) {
    static_assert(
        not std::is_assignable_v<FilePath, CStringView>, "assigning temporary is evil");
    auto test = [](auto&& arg) {
        const string& str(arg);
        FilePath x = "";
        CStringView csv(arg);
        x = csv;
        EXPECT_EQ(x.size(), str.size());
        EXPECT_EQ(x.data(), str);
    };

    test("abc");
    test("");
    test("e");
}

// NOLINTNEXTLINE
TEST(FilePath, assignment_from_string) {
    auto test = [](auto&& arg) {
        static_assert(
            not std::is_assignable_v<FilePath, string>, "assigning temporary is evil");
        string str(arg);
        FilePath x = "";
        x = str;
        EXPECT_EQ(x.size(), str.size());
        EXPECT_EQ(x.data(), str);
    };

    test("abc");
    test("");
    test("e");
}

// NOLINTNEXTLINE
TEST(FilePath, assignment_from_inplace_buff) {
    auto test = [](auto&& arg) {
        static_assert(
            not std::is_assignable_v<FilePath, InplaceBuff<16>>,
            "assigning temporary is evil");
        string str(arg);
        InplaceBuff<16> ib(arg);
        FilePath x = "";
        x = ib;
        EXPECT_EQ(x.size(), str.size());
        EXPECT_EQ(x.data(), str);
    };

    test("abc");
    test("");
    test("e");
}

// NOLINTNEXTLINE
TEST(FilePath, operator_const_char_ptr) {
    auto test = [](auto&& arg) {
        const string& str(arg);
        const FilePath x = arg;
        EXPECT_EQ((const char*)x, str);
    };

    test("abc");
    test("");
    test("e");
}

// NOLINTNEXTLINE
TEST(FilePath, operator_to_cstr) {
    auto test = [](auto&& arg) {
        const string& str(arg);
        const FilePath x = arg;
        EXPECT_EQ(x.to_cstr(), str);
    };

    test("abc");
    test("");
    test("e");
}

// NOLINTNEXTLINE
TEST(FilePath, operator_to_str) {
    auto test = [](auto&& arg) {
        const string& str(arg);
        const FilePath x = arg;
        EXPECT_EQ(x.to_str(), str);
    };

    test("abc");
    test("");
    test("e");
}

// NOLINTNEXTLINE
TEST(FilePath, data) {
    auto test = [](auto&& arg) {
        const FilePath x = arg;
        EXPECT_EQ(x.data(), arg);
    };

    test("abc");
    test("");
    test("e");
}

// NOLINTNEXTLINE
TEST(FilePath, size) {
    auto test = [](auto&& arg) {
        const string& str(arg);
        const FilePath x = arg;
        EXPECT_EQ(x.size(), str.size());
    };

    test("abc");
    test("");
    test("e");
}

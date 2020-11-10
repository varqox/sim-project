#include "simlib/temporary_directory.hh"
#include "simlib/file_info.hh"

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

using std::string;
using ::testing::MatchesRegex;

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(temporary_directory, TemporaryDirectory) {
    TemporaryDirectory tmp_dir;
    EXPECT_EQ(tmp_dir.path(), "");
    EXPECT_EQ(tmp_dir.exists(), false);

    tmp_dir = TemporaryDirectory("/tmp/filesystem-test.XXXXXX");
    EXPECT_THAT(tmp_dir.path(), MatchesRegex("/tmp/filesystem-test\\..{6}/"));
    EXPECT_EQ(tmp_dir.exists(), true);
    EXPECT_EQ(is_directory(tmp_dir.path()), true);

    string path_to_test;
    {
        TemporaryDirectory other("filesystem-test2.XXXXXX");
        static_assert(not std::is_convertible_v<const char*, TemporaryDirectory>);
        EXPECT_EQ(is_directory(other.path()), true);
        EXPECT_THAT(other.path(), MatchesRegex("/.*/filesystem-test2\\..{6}/"));
        EXPECT_THAT(other.name(), MatchesRegex("filesystem-test2\\..{6}/"));
        EXPECT_THAT(other.sname(), MatchesRegex("filesystem-test2\\..{6}/"));

        string path = tmp_dir.path();
        string other_path = other.path();
        string other_name = other.name();
        string other_sname = other.sname();
        tmp_dir = std::move(other);
        static_assert(not std::is_assignable_v<TemporaryDirectory, const TemporaryDirectory&>);

        EXPECT_EQ(tmp_dir.exists(), true);
        EXPECT_EQ(other.exists(), false); // NOLINT(bugprone-use-after-move)
        EXPECT_EQ(is_directory(path), false);
        EXPECT_EQ(is_directory(other_path), true);

        EXPECT_EQ(tmp_dir.path(), other_path);
        EXPECT_EQ(tmp_dir.name(), other_name);
        EXPECT_EQ(tmp_dir.sname(), other_sname);
        EXPECT_EQ(other_name, other_sname);

        other = std::move(tmp_dir);
        path_to_test = std::move(other_path);
        EXPECT_EQ(other.exists(), true);
        EXPECT_EQ(is_directory(path_to_test), true);
        EXPECT_EQ(tmp_dir.exists(), false); // NOLINT(bugprone-use-after-move)
    }

    EXPECT_EQ(is_directory(path_to_test), false);
}

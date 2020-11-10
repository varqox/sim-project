#include "simlib/temporary_file.hh"
#include "get_file_permissions.hh"
#include "simlib/defer.hh"
#include "simlib/file_info.hh"
#include "simlib/file_manip.hh"
#include "simlib/file_perms.hh"
#include "simlib/string_traits.hh"

#include <fcntl.h>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

using std::string;
using ::testing::MatchesRegex;

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(temporary_file, TemporaryFile) {
    string path;
    {
        TemporaryFile tmp_file("/tmp/filesystem-test.XXXXXX");
        EXPECT_EQ(tmp_file.is_open(), true);
        EXPECT_THAT(tmp_file.path(), MatchesRegex("/tmp/filesystem-test\\..{6}"));
        path = tmp_file.path();
        EXPECT_TRUE(is_regular_file(path));

        TemporaryFile other;
        EXPECT_EQ(other.is_open(), false);
        other = std::move(tmp_file);
        EXPECT_EQ(other.path(), path);
        EXPECT_TRUE(is_regular_file(path));
    }
    EXPECT_FALSE(path_exists(path));

    {
        TemporaryFile tmp_file("filesystem-test.XXXXXX");
        EXPECT_EQ(tmp_file.is_open(), true);
        EXPECT_THAT(tmp_file.path(), MatchesRegex("filesystem-test\\..{6}"));
        path = tmp_file.path();
        EXPECT_TRUE(is_regular_file(path));
    }
    EXPECT_FALSE(path_exists(path));
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(create_unique_file, typical) {
    string path1 = "/tmp/create_unique_file_test.XXXX";
    string path2 = path1;
    auto opt =
        create_unique_file(-1, path1.data(), path1.size(), 4, O_RDONLY | O_CLOEXEC, S_0644);
    Defer cleaner1 = [&] { (void)remove(path1); };
    EXPECT_TRUE(opt.has_value());
    EXPECT_TRUE(path_exists(path1));
    EXPECT_TRUE(has_prefix(path1, "/tmp/create_unique_file_test."));
    EXPECT_EQ(get_file_permissions(path1), S_0644);

    opt = create_unique_file(-1, path2.data(), path2.size(), 4, O_WRONLY | O_CLOEXEC, S_0600);
    Defer cleaner2 = [&] { (void)remove(path2); };
    EXPECT_TRUE(opt.has_value());
    EXPECT_TRUE(path_exists(path2));
    EXPECT_TRUE(has_prefix(path2, "/tmp/create_unique_file_test."));
    EXPECT_EQ(get_file_permissions(path2), S_0600);
    EXPECT_NE(path1, path2);
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(create_unique_file, fail) {
    string path = "/tmp/create_unique_file_test.";
    string path_copy = path;
    auto opt =
        create_unique_file(-1, path.data(), path.size(), 0, O_RDONLY | O_CLOEXEC, S_0600);
    Defer cleaner = [&] { (void)remove(path); };
    EXPECT_TRUE(opt.has_value());
    EXPECT_TRUE(path_exists(path));
    EXPECT_EQ(path, path_copy);
    EXPECT_EQ(get_file_permissions(path), S_0600);

    opt = create_unique_file(-1, path.data(), path.size(), 0, O_WRONLY | O_CLOEXEC, S_0600);
    int err = errno;
    EXPECT_FALSE(opt.has_value());
    EXPECT_EQ(err, EEXIST);
}

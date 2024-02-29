#include <gtest/gtest.h>
#include <simlib/concat.hh>
#include <simlib/file_info.hh>
#include <simlib/file_manip.hh>
#include <simlib/temporary_directory.hh>

// NOLINTNEXTLINE
TEST(DISABLED_file_info, access) { // TODO:
}

// NOLINTNEXTLINE
TEST(DISABLED_file_info, path_exists) { // TODO:
}

// NOLINTNEXTLINE
TEST(DISABLED_file_info, is_regular_file) { // TODO:
}

// NOLINTNEXTLINE
TEST(DISABLED_file_info, is_directory) { // TODO:
}

// NOLINTNEXTLINE
TEST(DISABLED_file_info, get_file_size) { // TODO:
}

// NOLINTNEXTLINE
TEST(file_info, get_modification_time) {
    TemporaryDirectory tmp_dir("/tmp/filesystem-test.XXXXXX");
    auto path = concat(tmp_dir.path(), "abc");
    EXPECT_EQ(create_file(path), 0);
    auto mtime = get_modification_time(path);
    using std::chrono_literals::operator""s;
    EXPECT_TRUE(std::chrono::system_clock::now() - mtime < 2s);
}

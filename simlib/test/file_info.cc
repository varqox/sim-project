#include "simlib/file_info.hh"
#include "simlib/file_descriptor.hh"
#include "simlib/temporary_directory.hh"

#include <gtest/gtest.h>

TEST(DISABLED_file_info, access) { // TODO:
}

TEST(DISABLED_file_info, path_exists) { // TODO:
}

TEST(DISABLED_file_info, is_regular_file) { // TODO:
}

TEST(DISABLED_file_info, is_directory) { // TODO:
}

TEST(DISABLED_file_info, get_file_size) { // TODO:
}

TEST(file_info, get_modification_time) {
	TemporaryDirectory tmp_dir("/tmp/filesystem-test.XXXXXX");
	auto path = concat(tmp_dir.path(), "abc");
	FileDescriptor(path, O_CREAT);
	auto mtime = get_modification_time(path);
	using std::chrono_literals::operator""s;
	EXPECT_TRUE(std::chrono::system_clock::now() - mtime < 2s);
}

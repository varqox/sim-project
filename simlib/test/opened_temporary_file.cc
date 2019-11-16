#include "../include/opened_temporary_file.hh"
#include "../include/file_contents.hh"
#include "../include/file_info.hh"
#include "file_descriptor_exists.hh"

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

using std::string;
using ::testing::MatchesRegex;

TEST(opened_temporary_file, OpenedTemporaryFile) {
	string path;
	int fd;
	{
		OpenedTemporaryFile tmp_file("/tmp/filesystem-test.XXXXXX");
		EXPECT_EQ(tmp_file.is_open(), true);
		EXPECT_THAT(tmp_file.path(),
		            MatchesRegex("/tmp/filesystem-test\\..{6}"));
		path = tmp_file.path();
		EXPECT_TRUE(is_regular_file(path));
		EXPECT_TRUE(file_descriptor_exists(tmp_file));
		fd = tmp_file;
		EXPECT_EQ(get_file_size(path), 0);
		write_all_throw(fd, "a", 1);
		EXPECT_EQ(get_file_size(path), 1);

		OpenedTemporaryFile other;
		EXPECT_EQ(other.is_open(), false);
		other = std::move(tmp_file);
		EXPECT_EQ(other.path(), path);
		EXPECT_TRUE(is_regular_file(path));
		EXPECT_FALSE(file_descriptor_exists(tmp_file));
		EXPECT_TRUE(file_descriptor_exists(other));
		EXPECT_EQ(fd, other);
		EXPECT_EQ(get_file_size(path), 1);
	}
	EXPECT_FALSE(file_descriptor_exists(fd));
	EXPECT_FALSE(path_exists(path));

	{
		OpenedTemporaryFile tmp_file("filesystem-test.XXXXXX");
		EXPECT_EQ(tmp_file.is_open(), true);
		EXPECT_THAT(tmp_file.path(), MatchesRegex("filesystem-test\\..{6}"));
		path = tmp_file.path();
		EXPECT_TRUE(is_regular_file(path));
		EXPECT_TRUE(file_descriptor_exists(tmp_file));
		fd = tmp_file;
	}
	EXPECT_FALSE(file_descriptor_exists(fd));
	EXPECT_FALSE(path_exists(path));
}

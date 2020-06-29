#include "simlib/temporary_file.hh"
#include "simlib/file_info.hh"

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
		EXPECT_THAT(tmp_file.path(),
		            MatchesRegex("/tmp/filesystem-test\\..{6}"));
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

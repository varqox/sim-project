#include "simlib/working_directory.hh"
#include "simlib/temporary_directory.hh"

#include <gtest/gtest.h>

TEST(working_directory, DirectoryChanger) {
	TemporaryDirectory tmp_dir("/tmp/filesystem-test.XXXXXX");
	auto old_cwd = get_cwd();
	EXPECT_NE(old_cwd, tmp_dir.path());
	{
		DirectoryChanger dc(tmp_dir.path());
		EXPECT_EQ(tmp_dir.path(), get_cwd());
	}

	EXPECT_EQ(old_cwd, get_cwd());
	DirectoryChanger dc;
	EXPECT_EQ(old_cwd, get_cwd());
	{
		DirectoryChanger other(tmp_dir.path());
		EXPECT_EQ(tmp_dir.path(), get_cwd());
		dc = std::move(other);
	}

	EXPECT_EQ(tmp_dir.path(), get_cwd());
	{
		DirectoryChanger other = std::move(dc);
		EXPECT_EQ(tmp_dir.path(), get_cwd());
	}
	EXPECT_EQ(old_cwd, get_cwd());
}

TEST(DISABLED_working_directory, get_cwd) {
	// TODO: implement it
}

TEST(DISABLED_working_directory, chdir_to_executable_dirpath) {
	// TODO: implement it
}

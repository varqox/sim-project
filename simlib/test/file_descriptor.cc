#include "simlib/file_descriptor.hh"
#include "file_descriptor_exists.hh"
#include "simlib/file_info.hh"
#include "simlib/temporary_directory.hh"

#include <gtest/gtest.h>

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(file_descriptor, FileDescriptor) {
	TemporaryDirectory tmp_dir("/tmp/filesystem-test.XXXXXX");
	static_assert(not std::is_convertible_v<int, FileDescriptor>);

	auto fd_a_path = concat_tostr(tmp_dir.path(), "abc");
	FileDescriptor fd_a(fd_a_path, O_CREAT | O_RDWR, S_0644);
	EXPECT_TRUE(is_regular_file(fd_a_path));
	EXPECT_TRUE(fd_a.is_open());

	static_assert(std::is_convertible_v<FileDescriptor, int>);

	{
		int fd = fd_a;
		EXPECT_TRUE(file_descriptor_exists(fd));
		EXPECT_EQ(fd_a.close(), 0);
		EXPECT_FALSE(file_descriptor_exists(fd));
		EXPECT_LT(fd_a, 0);
		EXPECT_FALSE(fd_a.is_open());
	}

	fd_a.open(fd_a_path, O_RDONLY);
	EXPECT_TRUE(fd_a.is_open());
	int fd = fd_a.release();
	EXPECT_FALSE(fd_a.is_open());
	EXPECT_TRUE(file_descriptor_exists(fd));
	{
		FileDescriptor fd_b(fd);
		EXPECT_TRUE(fd_b.is_open());

		fd_a = std::move(fd_b);
		EXPECT_TRUE(fd_a.is_open());
		EXPECT_FALSE(fd_b.is_open()); // NOLINT(bugprone-use-after-move)

		EXPECT_TRUE(file_descriptor_exists(fd));
		fd_b.reset(fd_a.release());
	}

	EXPECT_FALSE(fd_a.is_open());
	EXPECT_FALSE(file_descriptor_exists(fd));
}

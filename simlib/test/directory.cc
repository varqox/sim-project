#include "simlib/directory.hh"
#include "simlib/file_manip.hh"
#include "simlib/repeating.hh"
#include "simlib/temporary_directory.hh"

#include <gtest/gtest.h>

using std::string;

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(directory, for_each_dir_component) {
	TemporaryDirectory tmp_dir("/tmp/filesystem-test.XXXXXX");
	Directory dir(tmp_dir.path());
	static_assert(not std::is_convertible_v<string, Directory>);
	static_assert(not std::is_convertible_v<DIR*, Directory>);

	EXPECT_EQ(create_file(concat(tmp_dir.path(), "a")), 0);
	EXPECT_EQ(create_file(concat(tmp_dir.path(), "b")), 0);
	EXPECT_EQ(create_file(concat(tmp_dir.path(), "c")), 0);
	EXPECT_EQ(create_file(concat(tmp_dir.path(), "abc")), 0);
	EXPECT_EQ(create_file(concat(tmp_dir.path(), "xyz")), 0);

	std::set<string> files;
	for_each_dir_component(dir,
	                       [&](dirent* file) { files.emplace(file->d_name); });
	EXPECT_EQ(files, std::set<string>({"a", "b", "c", "abc", "xyz"}));

	files.clear();
	for_each_dir_component(tmp_dir.path(),
	                       [&](dirent* file) { files.emplace(file->d_name); });
	EXPECT_EQ(files, std::set<string>({"a", "b", "c", "abc", "xyz"}));

	int k = 0;
	dir.rewind();
	for_each_dir_component(dir, [&, iters = 2](dirent* /*unused*/) mutable {
		++k;
		return --iters > 0 ? continue_repeating : stop_repeating;
	});
	EXPECT_EQ(k, 2);

	k = 0;
	for_each_dir_component(
	   tmp_dir.path(), [&, iters = 2](dirent* /*unused*/) mutable {
		   ++k;
		   return --iters > 0 ? continue_repeating : stop_repeating;
	   });
	EXPECT_EQ(k, 2);
}

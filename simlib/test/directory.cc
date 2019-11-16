#include "../include/directory.hh"
#include "../include/file_descriptor.hh"
#include "../include/temporary_directory.hh"

#include <gtest/gtest.h>

using std::string;

TEST(directory, for_each_dir_component) {
	TemporaryDirectory tmp_dir("/tmp/filesystem-test.XXXXXX");
	Directory dir(tmp_dir.path());
	static_assert(not std::is_convertible_v<string, Directory>);
	static_assert(not std::is_convertible_v<DIR*, Directory>);

	FileDescriptor(concat(tmp_dir.path(), "a"), O_CREAT);
	FileDescriptor(concat(tmp_dir.path(), "b"), O_CREAT);
	FileDescriptor(concat(tmp_dir.path(), "c"), O_CREAT);
	FileDescriptor(concat(tmp_dir.path(), "abc"), O_CREAT);
	FileDescriptor(concat(tmp_dir.path(), "xyz"), O_CREAT);

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
	for_each_dir_component(dir, [&, iters = 2](dirent*) mutable {
		++k;
		return (--iters > 0);
	});
	EXPECT_EQ(k, 2);

	k = 0;
	for_each_dir_component(tmp_dir.path(), [&, iters = 2](dirent*) mutable {
		++k;
		return (--iters > 0);
	});
	EXPECT_EQ(k, 2);
}

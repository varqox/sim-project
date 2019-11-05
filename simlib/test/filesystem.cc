#include "../include/filesystem.hh"
#include "../include/defer.hh"
#include "../include/process.hh"
#include "../include/random.hh"
#include "../include/time.hh"

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

using std::max;
using std::pair;
using std::string;
using std::vector;
using ::testing::MatchesRegex;

static bool file_descriptor_exists(int fd) noexcept {
	return (fcntl(fd, F_GETFD) != -1);
}

static mode_t get_file_permissions(FilePath path) {
	struct stat64 st;
	EXPECT_EQ(stat64(path, &st), 0);
	return st.st_mode & ACCESSPERMS;
}

static string some_random_data(size_t len) {
	string data(len, '0');
	read_from_dev_urandom(data.data(), data.size());
	return data;
}

TEST(filesystem, FileDescriptor) {
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
		fd_a.close();
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
		EXPECT_FALSE(fd_b.is_open());

		EXPECT_TRUE(file_descriptor_exists(fd));
		fd_b.reset(fd_a.release());
	}

	EXPECT_FALSE(fd_a.is_open());
	EXPECT_FALSE(file_descriptor_exists(fd));
}

TEST(filesystem, TemporaryDirectory) {
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
		static_assert(
		   not std::is_convertible_v<const char*, TemporaryDirectory>);
		EXPECT_EQ(is_directory(other.path()), true);
		EXPECT_THAT(other.path(), MatchesRegex("/.*/filesystem-test2\\..{6}/"));
		EXPECT_THAT(other.name(), MatchesRegex("filesystem-test2\\..{6}/"));
		EXPECT_THAT(other.sname(), MatchesRegex("filesystem-test2\\..{6}/"));

		string path = tmp_dir.path();
		string other_path = other.path();
		string other_name = other.name();
		string other_sname = other.sname();
		tmp_dir = std::move(other);
		static_assert(not std::is_assignable_v<TemporaryDirectory,
		                                       const TemporaryDirectory&>);

		EXPECT_EQ(tmp_dir.exists(), true);
		EXPECT_EQ(other.exists(), false);
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
		EXPECT_EQ(tmp_dir.exists(), false);
	}

	EXPECT_EQ(is_directory(path_to_test), false);
}

TEST(filesystem, mkdir) {
	TemporaryDirectory tmp_dir("/tmp/filesystem-test.XXXXXX");
	EXPECT_EQ(mkdir(concat(tmp_dir.path(), "a")), 0);
	EXPECT_TRUE(is_directory(concat(tmp_dir.path(), "a")));

	EXPECT_EQ(mkdir(concat(tmp_dir.path(), "a/b")), 0);
	EXPECT_TRUE(is_directory(concat(tmp_dir.path(), "a/b")));

	EXPECT_EQ(mkdir(concat(tmp_dir.path(), "x/d")), -1);
	EXPECT_FALSE(is_directory(concat(tmp_dir.path(), "x")));
}

TEST(filesystem, mkdir_r) {
	TemporaryDirectory tmp_dir("/tmp/filesystem-test.XXXXXX");
	EXPECT_EQ(mkdir_r(concat_tostr(tmp_dir.path(), "x/d")), 0);
	EXPECT_TRUE(is_directory(concat(tmp_dir.path(), "x")));
	EXPECT_TRUE(is_directory(concat(tmp_dir.path(), "x/d")));
}

TEST(filesystem, TemporaryFile) {
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

TEST(filesystem, OpenedTemporaryFile) {
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

TEST(filesystem, DirectoryChanger) {
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

TEST(filesystem, for_each_dir_component) {
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

TEST(filesystem, remove_dir_contents) {
	TemporaryDirectory tmp_dir("/tmp/filesystem-test.XXXXXX");

	FileDescriptor(concat(tmp_dir.path(), "a"), O_CREAT);
	FileDescriptor(concat(tmp_dir.path(), "b"), O_CREAT);
	FileDescriptor(concat(tmp_dir.path(), "c"), O_CREAT);
	FileDescriptor(concat(tmp_dir.path(), "abc"), O_CREAT);
	FileDescriptor(concat(tmp_dir.path(), "xyz"), O_CREAT);
	mkdir_r(concat_tostr(tmp_dir.path(), "a/b/c/dd/"));
	FileDescriptor(concat(tmp_dir.path(), "a/b/c/dd/x"), O_CREAT);
	FileDescriptor(concat(tmp_dir.path(), "a/b/c/x"), O_CREAT);
	FileDescriptor(concat(tmp_dir.path(), "a/b/x"), O_CREAT);
	FileDescriptor(concat(tmp_dir.path(), "a/x"), O_CREAT);

	remove_dir_contents(tmp_dir.path());

	for_each_dir_component(tmp_dir.path(),
	                       [](dirent* f) { ADD_FAILURE() << f->d_name; });
}

TEST(filesystem, create_subdirectories) {
	TemporaryDirectory tmp_dir("/tmp/filesystem-test.XXXXXX");

	auto file_path = concat(tmp_dir.path(), "a/b/c/d/file.txt");
	auto dir_path = concat(tmp_dir.path(), "a/b/c/d/");

	create_subdirectories(file_path);
	EXPECT_TRUE(path_exists(dir_path));
	for_each_dir_component(dir_path,
	                       [](dirent* f) { ADD_FAILURE() << f->d_name; });
	EXPECT_FALSE(path_exists(file_path));

	remove_dir_contents(tmp_dir.path());
	create_subdirectories(dir_path);
	EXPECT_TRUE(path_exists(dir_path));
	for_each_dir_component(dir_path,
	                       [](dirent* f) { ADD_FAILURE() << f->d_name; });
}

TEST(filesystem, blast) {
	OpenedTemporaryFile a("/tmp/filesystem-test.XXXXXX");
	OpenedTemporaryFile b("/tmp/filesystem-test.XXXXXX");

	string data = some_random_data(1 << 20);
	write_all_throw(a, data);

	EXPECT_EQ(get_file_size(a.path()), data.size());
	EXPECT_EQ(get_file_size(b.path()), 0);

	EXPECT_EQ(lseek(a, 0, SEEK_SET), 0);
	EXPECT_EQ(blast(a, b), 0);

	EXPECT_EQ(get_file_size(a.path()), data.size());
	EXPECT_EQ(get_file_size(b.path()), data.size());

	string b_data = get_file_contents(b.path());
	EXPECT_EQ(data.size(), b_data.size());
	EXPECT_TRUE(data == b_data);

	string str = "abcdefghij";
	EXPECT_EQ(pwrite(a, str.data(), str.size(), 0), str.size());
	string other = "0123456789";
	EXPECT_EQ(pwrite(b, other.data(), other.size(), 0), other.size());

	EXPECT_EQ(lseek(a, 3, SEEK_SET), 3);
	EXPECT_EQ(ftruncate(a, 6), 0);
	EXPECT_EQ(lseek(b, 1, SEEK_SET), 1);

	EXPECT_EQ(blast(a, b), 0);
	EXPECT_EQ(get_file_size(b.path()), data.size());
	EXPECT_EQ(pread(b, other.data(), other.size(), 0), other.size());
	EXPECT_EQ(other, "0def456789");

	EXPECT_EQ(blast(a, b), 0); // Nothing should happen now
	EXPECT_EQ(get_file_size(b.path()), data.size());
	EXPECT_EQ(pread(b, other.data(), other.size(), 0), other.size());
	EXPECT_EQ(other, "0def456789");

	// Copying from already read out fd is no-op for dest fd
	EXPECT_EQ(blast(a, -1), 0);

	EXPECT_EQ(blast(-1, b), -1);
	EXPECT_EQ(blast(b, -1), -1);
	EXPECT_EQ(get_file_size(b.path()), data.size());
}

TEST(filesystem, copy) {
	TemporaryDirectory tmp_dir("/tmp/filesystem-test.XXXXXX");
	OpenedTemporaryFile a("/tmp/filesystem-test.XXXXXX");

	string data = some_random_data(1 << 18);
	write_all_throw(a, data);

	string bigger_data = some_random_data(1 << 19);
	string smaller_data = some_random_data(1 << 17);

	std::set<string> allowed_files;
	auto check_allowed_files = [&](size_t line) {
		size_t k = 0;
		for_each_dir_component(tmp_dir.path(), [&](dirent* f) {
			++k;
			if (allowed_files.count(concat_tostr(tmp_dir.path(), f->d_name)) ==
			    0) {
				ADD_FAILURE() << f->d_name << " (at line: " << line << ')';
			}
		});
		EXPECT_EQ(k, allowed_files.size()) << " (at line: " << line << ')';
	};

	EXPECT_EQ(get_file_size(a.path()), data.size());
	check_allowed_files(__LINE__);

	string b_path = concat_tostr(tmp_dir.path(), "bbb");
	allowed_files.emplace(b_path);
	EXPECT_EQ(::copy(a.path(), b_path, S_0644), 0);
	EXPECT_EQ(get_file_size(b_path), data.size());
	EXPECT_TRUE(get_file_contents(b_path) == data);
	EXPECT_EQ(get_file_permissions(b_path), S_0644);
	check_allowed_files(__LINE__);

	string c_path = concat_tostr(tmp_dir.path(), "ccc");
	allowed_files.emplace(c_path);
	EXPECT_EQ(::copy(a.path(), c_path, S_0755), 0);
	EXPECT_EQ(get_file_size(c_path), data.size());
	EXPECT_TRUE(get_file_contents(c_path) == data);
	EXPECT_EQ(get_file_permissions(c_path), S_0755);
	check_allowed_files(__LINE__);

	EXPECT_EQ(lseek(a, 0, SEEK_SET), 0);
	write_all_throw(a, bigger_data);
	EXPECT_EQ(::copy(a.path(), b_path, S_0755), 0);
	EXPECT_EQ(get_file_size(b_path), bigger_data.size());
	EXPECT_TRUE(get_file_contents(b_path) == bigger_data);
	EXPECT_EQ(get_file_permissions(b_path), S_0644);
	check_allowed_files(__LINE__);

	EXPECT_EQ(lseek(a, 0, SEEK_SET), 0);
	write_all_throw(a, smaller_data);
	EXPECT_EQ(ftruncate(a, smaller_data.size()), 0);

	EXPECT_EQ(::copy(a.path(), c_path, S_0644), 0);
	EXPECT_EQ(get_file_size(c_path), smaller_data.size());
	EXPECT_TRUE(get_file_contents(c_path) == smaller_data);
	EXPECT_EQ(get_file_permissions(c_path), S_0755);
	check_allowed_files(__LINE__);
}

TEST(filesystem, copy_r) {
	TemporaryDirectory tmp_dir("/tmp/filesystem-test.XXXXXX");

	struct FileInfo {
		string path;
		string data;

		bool operator<(const FileInfo& fi) const {
			return pair(path, data) < pair(fi.path, fi.data);
		}

		bool operator==(const FileInfo& fi) const {
			return (path == fi.path and data == fi.data);
		}
	};

	vector<FileInfo> orig_files = {
	   {"a", some_random_data(1023)},
	   {"b", some_random_data(1024)},
	   {"c", some_random_data(1025)},
	   {"dir/a", some_random_data(1023)},
	   {"dir/aa", some_random_data(100000)},
	   {"dir/b", some_random_data(1024)},
	   {"dir/bb", some_random_data(100000)},
	   {"dir/c", some_random_data(1025)},
	   {"dir/cc", some_random_data(100000)},
	   {"dir/dir/a", some_random_data(1023)},
	   {"dir/dir/aa", some_random_data(100000)},
	   {"dir/dir/b", some_random_data(1024)},
	   {"dir/dir/bb", some_random_data(100000)},
	   {"dir/dir/c", some_random_data(1025)},
	   {"dir/dir/cc", some_random_data(100000)},
	   {"dir/dir/xxx/a", some_random_data(1023)},
	   {"dir/dir/xxx/aa", some_random_data(100000)},
	   {"dir/dir/xxx/b", some_random_data(1024)},
	   {"dir/dir/xxx/bb", some_random_data(100000)},
	   {"dir/dir/xxx/c", some_random_data(1025)},
	   {"dir/dir/xxx/cc", some_random_data(100000)},
	   {"dir/dur/a", some_random_data(1023)},
	   {"dir/dur/aa", some_random_data(100000)},
	   {"dir/dur/b", some_random_data(1024)},
	   {"dir/dur/bb", some_random_data(100000)},
	   {"dir/dur/c", some_random_data(1025)},
	   {"dir/dur/cc", some_random_data(100000)},
	   {"dir/dur/xxx/a", some_random_data(1023)},
	   {"dir/dur/xxx/aa", some_random_data(100000)},
	   {"dir/dur/xxx/b", some_random_data(1024)},
	   {"dir/dur/xxx/bb", some_random_data(100000)},
	   {"dir/dur/xxx/c", some_random_data(1025)},
	   {"dir/dur/xxx/cc", some_random_data(100000)},
	};

	throw_assert(is_sorted(orig_files));

	for (auto& [path, data] : orig_files) {
		auto full_path = concat(tmp_dir.path(), path);
		create_subdirectories(full_path);
		put_file_contents(full_path, data);
	}

	auto dump_files = [&](FilePath path) {
		InplaceBuff<PATH_MAX> prefix = {path, '/'};

		vector<FileInfo> res;
		InplaceBuff<PATH_MAX> curr_path;
		auto process_dir = [&](auto& self) -> void {
			for_each_dir_component(
			   concat(prefix, curr_path), [&](dirent* file) {
				   Defer undoer = [&, old_len = curr_path.size] {
					   curr_path.size = old_len;
				   };
				   curr_path.append(file->d_name);
				   if (is_directory(concat(prefix, curr_path))) {
					   curr_path.append('/');
					   self(self);
				   } else {
					   res.push_back(
					      {curr_path.to_string(),
					       get_file_contents(concat(prefix, curr_path))});
				   }
			   });
		};

		process_dir(process_dir);
		sort(res);
		return res;
	};

	auto orig_files_slice = [&](StringView prefix) {
		vector<FileInfo> res;
		for (auto& [path, data] : orig_files) {
			if (has_prefix(path, prefix))
				res.push_back({path.substr(prefix.size()), data});
		}

		return res;
	};

	auto check_equality = [&](const vector<FileInfo>& fir,
	                          const vector<FileInfo>& sec, size_t line) {
		size_t len = max(fir.size(), sec.size());
		for (size_t i = 0; i < len; ++i) {
			if (i < fir.size() and i < sec.size()) {
				if (not(fir[i] == sec[i]))
					ADD_FAILURE_AT(__FILE__, line)
					   << "Unequal files:\t" << fir[i].path << "\t"
					   << sec[i].path;

				continue;
			} else if (i < fir.size()) {
				ADD_FAILURE_AT(__FILE__, line)
				   << "Extra file in fir:\t" << fir[i].path;
			} else {
				ADD_FAILURE_AT(__FILE__, line)
				   << "Extra file in sec:\t" << sec[i].path;
			}
		}
	};

	// Typical two levels
	{
		TemporaryDirectory dest_dir("/tmp/filesystem-test.XXXXXX");
		auto dest_path = concat(dest_dir.path(), "dest/dir");
		EXPECT_EQ(copy_r(concat(tmp_dir.path(), "dir"), dest_path), 0);
		check_equality(dump_files(dest_path), orig_files_slice("dir/"),
		               __LINE__);
	}

	// Typical one level
	{
		TemporaryDirectory dest_dir("/tmp/filesystem-test.XXXXXX");
		auto dest_path = concat(dest_dir.path(), "dest");
		EXPECT_EQ(copy_r(concat(tmp_dir.path(), "dir"), dest_path), 0);
		check_equality(dump_files(dest_path), orig_files_slice("dir/"),
		               __LINE__);
	}

	// Typical into existing
	{
		TemporaryDirectory dest_dir("/tmp/filesystem-test.XXXXXX");
		auto dest_path = dest_dir.path();
		EXPECT_EQ(copy_r(concat(tmp_dir.path(), "dir"), dest_path), 0);
		check_equality(dump_files(dest_path), orig_files_slice("dir/"),
		               __LINE__);
	}

	// Without creating subdirs into existing
	{
		TemporaryDirectory dest_dir("/tmp/filesystem-test.XXXXXX");
		auto dest_path = dest_dir.path();
		EXPECT_EQ(copy_r(concat(tmp_dir.path(), "dir"), dest_path, false), 0);
		check_equality(dump_files(dest_path), orig_files_slice("dir/"),
		               __LINE__);
	}

	// Without creating subdirs one level
	{
		TemporaryDirectory dest_dir("/tmp/filesystem-test.XXXXXX");
		auto dest_path = concat(dest_dir.path(), "dest/");
		EXPECT_EQ(copy_r(concat(tmp_dir.path(), "dir"), dest_path, false), 0);
		check_equality(dump_files(dest_path), orig_files_slice("dir/"),
		               __LINE__);
	}

	// Without creating subdirs two levels
	{
		TemporaryDirectory dest_dir("/tmp/filesystem-test.XXXXXX");
		auto dest_path = concat(dest_dir.path(), "dest/dir");
		errno = 0;
		EXPECT_EQ(copy_r(concat(tmp_dir.path(), "dir"), dest_path, false), -1);
		EXPECT_EQ(errno, ENOENT);
		EXPECT_FALSE(path_exists(dest_path));
	}
}

TEST(DISABLED_filesystem, move) { // TODO:
}

TEST(DISABLED_filesystem, create_file) { // TODO:
}

TEST(DISABLED_filesystem, read_all) { // TODO:
}

TEST(DISABLED_filesystem, write_all) { // TODO:
}

TEST(DISABLED_filesystem, get_file_contents_fd) { // TODO:
}

TEST(DISABLED_filesystem, get_file_contents_fd_with_offsets) { // TODO:
}

TEST(DISABLED_filesystem, get_file_contents_path) { // TODO:
}

TEST(DISABLED_filesystem, get_file_contents_path_with_offsets) { // TODO:
}

TEST(DISABLED_filesystem, put_file_contents) { // TODO:
}

TEST(DISABLED_filesystem, FileRemover) { // TODO:
}

TEST(DISABLED_filesystem, DirectoryRemover) { // TODO:
}

TEST(filesystem, humanize_file_size) {
	EXPECT_EQ(humanize_file_size(0), "0 bytes");
	EXPECT_EQ(humanize_file_size(1), "1 byte");
	EXPECT_EQ(humanize_file_size(2), "2 bytes");
	EXPECT_EQ(humanize_file_size(1023), "1023 bytes");
	EXPECT_EQ(humanize_file_size(1024), "1.0 KiB");
	EXPECT_EQ(humanize_file_size(129747), "127 KiB");
	EXPECT_EQ(humanize_file_size(97379112), "92.9 MiB");
	EXPECT_EQ(humanize_file_size(1.423 * (1ull << 10)), "1.4 KiB");
	EXPECT_EQ(humanize_file_size(14.23 * (1ull << 10)), "14.2 KiB");
	EXPECT_EQ(humanize_file_size(142.3 * (1ull << 10)), "142 KiB");
	EXPECT_EQ(humanize_file_size(1.423 * (1ull << 20)), "1.4 MiB");
	EXPECT_EQ(humanize_file_size(14.23 * (1ull << 20)), "14.2 MiB");
	EXPECT_EQ(humanize_file_size(142.3 * (1ull << 20)), "142 MiB");
	EXPECT_EQ(humanize_file_size(1.423 * (1ull << 30)), "1.4 GiB");
	EXPECT_EQ(humanize_file_size(14.23 * (1ull << 30)), "14.2 GiB");
	EXPECT_EQ(humanize_file_size(142.3 * (1ull << 30)), "142 GiB");
	EXPECT_EQ(humanize_file_size(1.423 * (1ull << 40)), "1.4 TiB");
	EXPECT_EQ(humanize_file_size(14.23 * (1ull << 40)), "14.2 TiB");
	EXPECT_EQ(humanize_file_size(142.3 * (1ull << 40)), "142 TiB");
	EXPECT_EQ(humanize_file_size(1.423 * (1ull << 50)), "1.4 PiB");
	EXPECT_EQ(humanize_file_size(14.23 * (1ull << 50)), "14.2 PiB");
	EXPECT_EQ(humanize_file_size(142.3 * (1ull << 50)), "142 PiB");
	EXPECT_EQ(humanize_file_size(1.423 * (1ull << 60)), "1.4 EiB");
	EXPECT_EQ(humanize_file_size(14.23 * (1ull << 60)), "14.2 EiB");

	EXPECT_EQ(humanize_file_size((1ull << 10) - 1), "1023 bytes");
	EXPECT_EQ(humanize_file_size((1ull << 10)), "1.0 KiB");
	EXPECT_EQ(humanize_file_size((1ull << 20) - 1), "1024 KiB");
	EXPECT_EQ(humanize_file_size((1ull << 20)), "1.0 MiB");
	EXPECT_EQ(humanize_file_size((1ull << 30) - 1), "1024 MiB");
	EXPECT_EQ(humanize_file_size((1ull << 30)), "1.0 GiB");
	EXPECT_EQ(humanize_file_size((1ull << 40) - 1), "1024 GiB");
	EXPECT_EQ(humanize_file_size((1ull << 40)), "1.0 TiB");
	EXPECT_EQ(humanize_file_size((1ull << 50) - 1), "1024 TiB");
	EXPECT_EQ(humanize_file_size((1ull << 50)), "1.0 PiB");
	EXPECT_EQ(humanize_file_size((1ull << 60) - 1), "1024 PiB");
	EXPECT_EQ(humanize_file_size((1ull << 60)), "1.0 EiB");

	EXPECT_EQ(humanize_file_size(102349ull - 1), "99.9 KiB");
	EXPECT_EQ(humanize_file_size(102349ull), "100 KiB");
	EXPECT_EQ(humanize_file_size(104805172ull - 1), "99.9 MiB");
	EXPECT_EQ(humanize_file_size(104805172ull), "100 MiB");
	EXPECT_EQ(humanize_file_size(107320495309ull - 1), "99.9 GiB");
	EXPECT_EQ(humanize_file_size(107320495309ull), "100 GiB");
	EXPECT_EQ(humanize_file_size(109896187196212ull - 1), "99.9 TiB");
	EXPECT_EQ(humanize_file_size(109896187196212ull), "100 TiB");
	EXPECT_EQ(humanize_file_size(112533595688920269ull - 1), "99.9 PiB");
	EXPECT_EQ(humanize_file_size(112533595688920269ull), "100 PiB");
}

TEST(DISABLED_filesystem, path_exists) { // TODO:
}

TEST(DISABLED_filesystem, is_regular_file) { // TODO:
}

TEST(DISABLED_filesystem, is_directory) { // TODO:
}

TEST(DISABLED_filesystem, get_file_size) { // TODO:
}

TEST(filesystem, get_modification_time) {
	TemporaryDirectory tmp_dir("/tmp/filesystem-test.XXXXXX");
	auto path = concat(tmp_dir.path(), "abc");
	FileDescriptor(path, O_CREAT);
	auto mtime = get_modification_time(path);
	using std::chrono_literals::operator""s;
	EXPECT_TRUE(std::chrono::system_clock::now() - mtime < 2s);
}

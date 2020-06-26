#include "simlib/path.hh"

#include <gtest/gtest.h>

TEST(path, path_absolute) {
	EXPECT_EQ(path_absolute("/foo/bar/"), "/foo/bar/");
	EXPECT_EQ(path_absolute("/foo/bar/////"), "/foo/bar/");
	EXPECT_EQ(path_absolute("/foo/bar/../"), "/foo/");
	EXPECT_EQ(path_absolute("/foo/bar/.."), "/foo/");
	EXPECT_EQ(path_absolute("/foo/bar/xd"), "/foo/bar/xd");
	EXPECT_EQ(path_absolute("/foo/bar/////xd"), "/foo/bar/xd");
	EXPECT_EQ(path_absolute("/foo/bar/../xd"), "/foo/xd");
	EXPECT_EQ(path_absolute("/foo/bar/../xd/."), "/foo/xd/");
	EXPECT_EQ(path_absolute("/foo/bar/./../xd/."), "/foo/xd/");
	EXPECT_EQ(path_absolute("/.."), "/");
	EXPECT_EQ(path_absolute(".."), "/");
	EXPECT_EQ(path_absolute("/////"), "/");
	EXPECT_EQ(path_absolute("////a/////"), "/a/");

	EXPECT_EQ(path_absolute("/my/path/foo.bar"), "/my/path/foo.bar");
	EXPECT_EQ(path_absolute("/my/path/"), "/my/path/");
	EXPECT_EQ(path_absolute("/"), "/");
	EXPECT_EQ(path_absolute(""), "/");
	EXPECT_EQ(path_absolute("/my/path/."), "/my/path/");
	EXPECT_EQ(path_absolute("/my/path/.."), "/my/");
	EXPECT_EQ(path_absolute("foo"), "/foo");
	EXPECT_EQ(path_absolute("/lol/../foo."), "/foo.");
	EXPECT_EQ(path_absolute("/lol/../.foo."), "/.foo.");
	EXPECT_EQ(path_absolute("/.foo."), "/.foo.");
	EXPECT_EQ(path_absolute(".foo."), "/.foo.");
	EXPECT_EQ(path_absolute("./foo."), "/foo.");
	EXPECT_EQ(path_absolute("./f"), "/f");
	EXPECT_EQ(path_absolute("../"), "/");

	EXPECT_EQ(path_absolute("../../a", "foo/bar"), "a");
	EXPECT_EQ(path_absolute("../../a/../b", "foo/bar"), "b");
	EXPECT_EQ(path_absolute("../", "foo/bar"), "foo/");
	EXPECT_EQ(path_absolute("gg", "foo/bar"), "foo/bar/gg");
}

TEST(path, path_filename) {
	EXPECT_EQ(path_filename("/my/path/foo.bar"), "foo.bar");
	EXPECT_EQ(path_filename("/my/path/"), "");
	EXPECT_EQ(path_filename("/"), "");
	EXPECT_EQ(path_filename(""), "");
	EXPECT_EQ(path_filename("/my/path/."), ".");
	EXPECT_EQ(path_filename("/my/path/.."), "..");
	EXPECT_EQ(path_filename("foo"), "foo");
	EXPECT_EQ(path_filename("/lol/../foo."), "foo.");
	EXPECT_EQ(path_filename("/lol/../.foo."), ".foo.");
	EXPECT_EQ(path_filename("/.foo."), ".foo.");
	EXPECT_EQ(path_filename(".foo."), ".foo.");
	EXPECT_EQ(path_filename("./foo."), "foo.");
	EXPECT_EQ(path_filename("./f"), "f");
	EXPECT_EQ(path_filename("../"), "");
}

TEST(path, path_extension) {
	EXPECT_EQ(path_extension("/my/path/foo.bar"), "bar");
	EXPECT_EQ(path_extension("/my/path/"), "");
	EXPECT_EQ(path_extension("/"), "");
	EXPECT_EQ(path_extension(""), "");
	EXPECT_EQ(path_extension("/my/path/."), "");
	EXPECT_EQ(path_extension("/my/path/.."), "");
	EXPECT_EQ(path_extension("foo"), "");
	EXPECT_EQ(path_extension("/lol/../foo."), "");
	EXPECT_EQ(path_extension("/lol/../.foo."), "");
	EXPECT_EQ(path_extension("/.foo."), "");
	EXPECT_EQ(path_extension(".foo."), "");
	EXPECT_EQ(path_extension("./foo."), "");
	EXPECT_EQ(path_extension("./f"), "");
	EXPECT_EQ(path_extension("../"), "");
	EXPECT_EQ(path_extension("foo.cc"), "cc");
	EXPECT_EQ(path_extension("bar"), "");
	EXPECT_EQ(path_extension(".foobar"), "foobar");
	EXPECT_EQ(path_extension("/abc/./aa/../foo.cc"), "cc");
	EXPECT_EQ(path_extension("/abc/./aa/../bar"), "");
	EXPECT_EQ(path_extension("/abc/./aa/../.foobar"), "foobar");
	EXPECT_EQ(path_extension("/abc/./aa/../.foobar/"), "");
	EXPECT_EQ(path_extension("abc/./aa/../foo.cc"), "cc");
	EXPECT_EQ(path_extension("abc/./aa/../bar"), "");
	EXPECT_EQ(path_extension("abc/./aa/../.foobar"), "foobar");
	EXPECT_EQ(path_extension("abc/./aa/../.foobar/"), "");
	EXPECT_EQ(path_extension("../foo.cc"), "cc");
	EXPECT_EQ(path_extension("../bar"), "");
	EXPECT_EQ(path_extension("../.foobar"), "foobar");
	EXPECT_EQ(path_extension("../.foobar/"), "");
	EXPECT_EQ(path_extension("./foo.cc"), "cc");
	EXPECT_EQ(path_extension("./bar"), "");
	EXPECT_EQ(path_extension("./.foobar"), "foobar");
	EXPECT_EQ(path_extension("./.foobar/"), "");
	EXPECT_EQ(path_extension("aaa/foo.cc"), "cc");
	EXPECT_EQ(path_extension("aaa/bar"), "");
	EXPECT_EQ(path_extension("aaa/.foobar"), "foobar");
	EXPECT_EQ(path_extension("aaa/.foobar/"), "");
	EXPECT_EQ(path_extension("/aaa/foo.cc"), "cc");
	EXPECT_EQ(path_extension("/aaa/bar"), "");
	EXPECT_EQ(path_extension("/aaa/.foobar"), "foobar");
	EXPECT_EQ(path_extension("/aaa/.foobar/"), "");
	EXPECT_EQ(path_extension("/./foo.cc"), "cc");
	EXPECT_EQ(path_extension("/./bar"), "");
	EXPECT_EQ(path_extension("/./.foobar"), "foobar");
	EXPECT_EQ(path_extension("/./.foobar/"), "");
	EXPECT_EQ(path_extension("/../foo.cc"), "cc");
	EXPECT_EQ(path_extension("/../bar"), "");
	EXPECT_EQ(path_extension("/../.foobar"), "foobar");
	EXPECT_EQ(path_extension("/../.foobar/"), "");
}

TEST(path, path_dirpath) {
	EXPECT_EQ(path_dirpath("/my/path/foo.bar"), "/my/path/");
	EXPECT_EQ(path_dirpath("/my/path/"), "/my/path/");
	EXPECT_EQ(path_dirpath("/"), "/");
	EXPECT_EQ(path_dirpath(""), "");
	EXPECT_EQ(path_dirpath("/my/path/."), "/my/path/");
	EXPECT_EQ(path_dirpath("/my/path/.."), "/my/path/");
	EXPECT_EQ(path_dirpath("foo"), "");
	EXPECT_EQ(path_dirpath("/lol/../foo."), "/lol/../");
	EXPECT_EQ(path_dirpath("/lol/../.foo."), "/lol/../");
	EXPECT_EQ(path_dirpath("/.foo."), "/");
	EXPECT_EQ(path_dirpath(".foo."), "");
	EXPECT_EQ(path_dirpath("./foo."), "./");
	EXPECT_EQ(path_dirpath("./f"), "./");
	EXPECT_EQ(path_dirpath("../"), "../");
	EXPECT_EQ(path_dirpath("foo.cc"), "");
	EXPECT_EQ(path_dirpath("bar"), "");
	EXPECT_EQ(path_dirpath(".foobar"), "");
	EXPECT_EQ(path_dirpath("/abc/./aa/../foo.cc"), "/abc/./aa/../");
	EXPECT_EQ(path_dirpath("/abc/./aa/../bar"), "/abc/./aa/../");
	EXPECT_EQ(path_dirpath("/abc/./aa/../.foobar"), "/abc/./aa/../");
	EXPECT_EQ(path_dirpath("/abc/./aa/../.foobar/"), "/abc/./aa/../.foobar/");
	EXPECT_EQ(path_dirpath("abc/./aa/../foo.cc"), "abc/./aa/../");
	EXPECT_EQ(path_dirpath("abc/./aa/../bar"), "abc/./aa/../");
	EXPECT_EQ(path_dirpath("abc/./aa/../.foobar"), "abc/./aa/../");
	EXPECT_EQ(path_dirpath("abc/./aa/../.foobar/"), "abc/./aa/../.foobar/");
	EXPECT_EQ(path_dirpath("../foo.cc"), "../");
	EXPECT_EQ(path_dirpath("../bar"), "../");
	EXPECT_EQ(path_dirpath("../.foobar"), "../");
	EXPECT_EQ(path_dirpath("../.foobar/"), "../.foobar/");
	EXPECT_EQ(path_dirpath("./foo.cc"), "./");
	EXPECT_EQ(path_dirpath("./bar"), "./");
	EXPECT_EQ(path_dirpath("./.foobar"), "./");
	EXPECT_EQ(path_dirpath("./.foobar/"), "./.foobar/");
	EXPECT_EQ(path_dirpath("aaa/foo.cc"), "aaa/");
	EXPECT_EQ(path_dirpath("aaa/bar"), "aaa/");
	EXPECT_EQ(path_dirpath("aaa/.foobar"), "aaa/");
	EXPECT_EQ(path_dirpath("aaa/.foobar/"), "aaa/.foobar/");
	EXPECT_EQ(path_dirpath("/aaa/foo.cc"), "/aaa/");
	EXPECT_EQ(path_dirpath("/aaa/bar"), "/aaa/");
	EXPECT_EQ(path_dirpath("/aaa/.foobar"), "/aaa/");
	EXPECT_EQ(path_dirpath("/aaa/.foobar/"), "/aaa/.foobar/");
	EXPECT_EQ(path_dirpath("/./foo.cc"), "/./");
	EXPECT_EQ(path_dirpath("/./bar"), "/./");
	EXPECT_EQ(path_dirpath("/./.foobar"), "/./");
	EXPECT_EQ(path_dirpath("/./.foobar/"), "/./.foobar/");
	EXPECT_EQ(path_dirpath("/../foo.cc"), "/../");
	EXPECT_EQ(path_dirpath("/../bar"), "/../");
	EXPECT_EQ(path_dirpath("/../.foobar"), "/../");
	EXPECT_EQ(path_dirpath("/../.foobar/"), "/../.foobar/");
}

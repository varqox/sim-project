#include "../include/filesystem.h"

#include <gtest/gtest.h>

TEST (Filesystem, abspath) {
	EXPECT_EQ(abspath("/foo/bar/"), "/foo/bar/");
	EXPECT_EQ(abspath("/foo/bar/////"), "/foo/bar/");
	EXPECT_EQ(abspath("/foo/bar/../"), "/foo/");
	EXPECT_EQ(abspath("/foo/bar/.."), "/foo/");
	EXPECT_EQ(abspath("/foo/bar/xd"), "/foo/bar/xd");
	EXPECT_EQ(abspath("/foo/bar/////xd"), "/foo/bar/xd");
	EXPECT_EQ(abspath("/foo/bar/../xd"), "/foo/xd");
	EXPECT_EQ(abspath("/foo/bar/../xd/."), "/foo/xd/");
	EXPECT_EQ(abspath("/foo/bar/./../xd/."), "/foo/xd/");
	EXPECT_EQ(abspath("/.."), "/");
	EXPECT_EQ(abspath(".."), "/");
	EXPECT_EQ(abspath("/////"), "/");
	EXPECT_EQ(abspath("////a/////"), "/a/");

	EXPECT_EQ(abspath("/my/path/foo.bar"), "/my/path/foo.bar");
	EXPECT_EQ(abspath("/my/path/"), "/my/path/");
	EXPECT_EQ(abspath("/"), "/");
	EXPECT_EQ(abspath(""), "/");
	EXPECT_EQ(abspath("/my/path/."), "/my/path/");
	EXPECT_EQ(abspath("/my/path/.."), "/my/");
	EXPECT_EQ(abspath("foo"), "/foo");
	EXPECT_EQ(abspath("/lol/../foo."), "/foo.");
	EXPECT_EQ(abspath("/lol/../.foo."), "/.foo.");
	EXPECT_EQ(abspath("/.foo."), "/.foo.");
	EXPECT_EQ(abspath(".foo."), "/.foo.");
	EXPECT_EQ(abspath("./foo."), "/foo.");
	EXPECT_EQ(abspath("./f"), "/f");
	EXPECT_EQ(abspath("../"), "/");

	EXPECT_EQ(abspath("../../a", 0, -1, "foo/bar"), "a");
	EXPECT_EQ(abspath("../../a/../b", 0, -1, "foo/bar"), "b");
	EXPECT_EQ(abspath("../", 0, -1, "foo/bar"), "foo/");
	EXPECT_EQ(abspath("gg", 0, -1, "foo/bar"), "foo/bar/gg");
}

TEST (Filesystem, filename) {
	EXPECT_EQ(filename("/my/path/foo.bar"), "foo.bar");
	EXPECT_EQ(filename("/my/path/"), "");
	EXPECT_EQ(filename("/"), "");
	EXPECT_EQ(filename(""), "");
	EXPECT_EQ(filename("/my/path/."), ".");
	EXPECT_EQ(filename("/my/path/.."), "..");
	EXPECT_EQ(filename("foo"), "foo");
	EXPECT_EQ(filename("/lol/../foo."), "foo.");
	EXPECT_EQ(filename("/lol/../.foo."), ".foo.");
	EXPECT_EQ(filename("/.foo."), ".foo.");
	EXPECT_EQ(filename(".foo."), ".foo.");
	EXPECT_EQ(filename("./foo."), "foo.");
	EXPECT_EQ(filename("./f"), "f");
	EXPECT_EQ(filename("../"), "");
}

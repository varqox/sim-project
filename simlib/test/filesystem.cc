#include "../include/filesystem.h"

#include <gtest/gtest.h>

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

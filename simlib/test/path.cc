#include "simlib/path.hh"
#include "simlib/concat_tostr.hh"
#include "simlib/file_manip.hh"
#include "simlib/string_traits.hh"
#include "simlib/temporary_directory.hh"

#include <gtest/gtest-death-test.h>
#include <gtest/gtest.h>

// NOLINTNEXTLINE
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

// NOLINTNEXTLINE
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

// NOLINTNEXTLINE
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

// NOLINTNEXTLINE
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

// NOLINTNEXTLINE
TEST(path, deepest_ancestor_dir_with_subpath_absolute_path) {
    TemporaryDirectory tmp_dir("/tmp/path.test.XXXXXX");
    StringView tmp_dir_pathname = StringView(tmp_dir.path()).without_trailing('/').remove_prefix(5);
    assert(has_prefix(tmp_dir_pathname, "path.test."));
    assert(tmp_dir_pathname.size() == 16);

    ASSERT_EQ(mkdir_r(tmp_dir.path() + "a/b/c/d/"), 0);
    ASSERT_EQ(create_file(tmp_dir.path() + "a/b/c/d/file"), 0);
    ASSERT_EQ(mkdir_r(tmp_dir.path() + "a/b/e/"), 0);
    ASSERT_EQ(create_file(tmp_dir.path() + "a/b/e/file"), 0);
    ASSERT_EQ(mkdir_r(tmp_dir.path() + "a/f/"), 0);
    ASSERT_EQ(create_file(tmp_dir.path() + "a/f/file"), 0);
    ASSERT_EQ(mkdir_r(tmp_dir.path() + "g/"), 0);
    ASSERT_EQ(create_file(tmp_dir.path() + "g/file"), 0);
    ASSERT_EQ(create_file(tmp_dir.path() + "file"), 0);

    EXPECT_EQ(deepest_ancestor_dir_with_subpath(tmp_dir.path() + "a/b/c/", "d/file"),
            tmp_dir.path() + "a/b/c/d/file");
    EXPECT_EQ(deepest_ancestor_dir_with_subpath(tmp_dir.path() + "a/b/c/", "e/file"),
            tmp_dir.path() + "a/b/e/file");
    EXPECT_EQ(deepest_ancestor_dir_with_subpath(tmp_dir.path() + "a/b/c/", "f/file"),
            tmp_dir.path() + "a/f/file");
    EXPECT_EQ(deepest_ancestor_dir_with_subpath(tmp_dir.path() + "a/b/c/", "g/file"),
            tmp_dir.path() + "g/file");
    EXPECT_EQ(deepest_ancestor_dir_with_subpath(tmp_dir.path() + "a/b/c/", "file"),
            tmp_dir.path() + "file");
    EXPECT_EQ(deepest_ancestor_dir_with_subpath(tmp_dir.path() + "a/b/c/", ""),
            tmp_dir.path() + "a/b/c/");
    EXPECT_EQ(deepest_ancestor_dir_with_subpath(tmp_dir.path() + "a/b/c/", "/"),
            tmp_dir.path() + "a/b/c/");
    EXPECT_EQ(deepest_ancestor_dir_with_subpath(tmp_dir.path() + "a/b/c/", "/f/file"),
            tmp_dir.path() + "a/f/file");
    EXPECT_EQ(deepest_ancestor_dir_with_subpath(tmp_dir.path() + "a/b/c/", "xxxxxxxxx"),
            std::nullopt);
    EXPECT_EQ(deepest_ancestor_dir_with_subpath(tmp_dir.path() + "a/b/c/", tmp_dir_pathname),
            concat_tostr("/tmp/", tmp_dir_pathname));
    EXPECT_EQ(deepest_ancestor_dir_with_subpath(tmp_dir.path() + "a/b/c/", "tmp"), "/tmp");
    EXPECT_EQ(deepest_ancestor_dir_with_subpath(tmp_dir.path() + "a/b/c/", "tmp/"), "/tmp/");

    EXPECT_EQ(deepest_ancestor_dir_with_subpath(tmp_dir.path() + "a/b/c", "d/file"), std::nullopt);
    EXPECT_EQ(deepest_ancestor_dir_with_subpath(tmp_dir.path() + "a/b/c", "e/file"),
            tmp_dir.path() + "a/b/e/file");
    EXPECT_EQ(deepest_ancestor_dir_with_subpath(tmp_dir.path() + "a/b/c", "f/file"),
            tmp_dir.path() + "a/f/file");
    EXPECT_EQ(deepest_ancestor_dir_with_subpath(tmp_dir.path() + "a/b/c", "g/file"),
            tmp_dir.path() + "g/file");
    EXPECT_EQ(deepest_ancestor_dir_with_subpath(tmp_dir.path() + "a/b/c", "file"),
            tmp_dir.path() + "file");
    EXPECT_EQ(deepest_ancestor_dir_with_subpath(tmp_dir.path() + "a/b/c", ""),
            tmp_dir.path() + "a/b/");
    EXPECT_EQ(deepest_ancestor_dir_with_subpath(tmp_dir.path() + "a/b/c", "/"),
            tmp_dir.path() + "a/b/");
    EXPECT_EQ(deepest_ancestor_dir_with_subpath(tmp_dir.path() + "a/b/c", "/f/file"),
            tmp_dir.path() + "a/f/file");
    EXPECT_EQ(
            deepest_ancestor_dir_with_subpath(tmp_dir.path() + "a/b/c", "xxxxxxxxx"), std::nullopt);
    EXPECT_EQ(deepest_ancestor_dir_with_subpath(tmp_dir.path() + "a/b/c", tmp_dir_pathname),
            concat_tostr("/tmp/", tmp_dir_pathname));
    EXPECT_EQ(deepest_ancestor_dir_with_subpath(tmp_dir.path() + "a/b/c", "tmp"), "/tmp");
    EXPECT_EQ(deepest_ancestor_dir_with_subpath(tmp_dir.path() + "a/b/c", "tmp/"), "/tmp/");
}

// NOLINTNEXTLINE
TEST(path_DeathTest, deepest_ancestor_dir_with_subpath_relative_path) {
    auto test_impl = [] {
        TemporaryDirectory tmp_dir("/tmp/path.test.XXXXXX");
        ASSERT_EQ(chdir(tmp_dir.path().data()),
                0); // That is why it is a death test
        ASSERT_EQ(mkdir_r("a/b/c/d/"), 0);
        ASSERT_EQ(create_file("a/b/c/d/file"), 0);
        ASSERT_EQ(mkdir_r("a/b/e/"), 0);
        ASSERT_EQ(create_file("a/b/e/file"), 0);
        ASSERT_EQ(mkdir_r("a/f/"), 0);
        ASSERT_EQ(create_file("a/f/file"), 0);
        ASSERT_EQ(mkdir_r("g/"), 0);
        ASSERT_EQ(create_file("g/file"), 0);
        ASSERT_EQ(create_file("file"), 0);

        EXPECT_EQ(deepest_ancestor_dir_with_subpath("a/b/c/", "d/file"), "a/b/c/d/file");
        EXPECT_EQ(deepest_ancestor_dir_with_subpath("a/b/c/", "e/file"), "a/b/e/file");
        EXPECT_EQ(deepest_ancestor_dir_with_subpath("a/b/c/", "f/file"), "a/f/file");
        EXPECT_EQ(deepest_ancestor_dir_with_subpath("a/b/c/", "g/file"), "g/file");
        EXPECT_EQ(deepest_ancestor_dir_with_subpath("a/b/c/", "file"), "file");
        EXPECT_EQ(deepest_ancestor_dir_with_subpath("a/b/c/", "g/"), "g/");
        EXPECT_EQ(deepest_ancestor_dir_with_subpath("a/b/c/", "g"), "g");
        EXPECT_EQ(deepest_ancestor_dir_with_subpath("a/b/c/", "/g/file"), "g/file");
        EXPECT_EQ(deepest_ancestor_dir_with_subpath("a/b/c/", ""), "a/b/c/");
        EXPECT_EQ(deepest_ancestor_dir_with_subpath("a/b/c/", "/"), "a/b/c/");
        EXPECT_EQ(deepest_ancestor_dir_with_subpath("a/b/c/", "x"), std::nullopt);

        EXPECT_EQ(deepest_ancestor_dir_with_subpath("a/b/c", "d/file"), std::nullopt);
        EXPECT_EQ(deepest_ancestor_dir_with_subpath("a/b/c", "e/file"), "a/b/e/file");
        EXPECT_EQ(deepest_ancestor_dir_with_subpath("a/b/c", "f/file"), "a/f/file");
        EXPECT_EQ(deepest_ancestor_dir_with_subpath("a/b/c", "g/file"), "g/file");
        EXPECT_EQ(deepest_ancestor_dir_with_subpath("a/b/c", "file"), "file");
        EXPECT_EQ(deepest_ancestor_dir_with_subpath("a/b/c", "g/"), "g/");
        EXPECT_EQ(deepest_ancestor_dir_with_subpath("a/b/c", "g"), "g");
        EXPECT_EQ(deepest_ancestor_dir_with_subpath("a/b/c", "/g/file"), "g/file");
        EXPECT_EQ(deepest_ancestor_dir_with_subpath("a/b/c", ""), "a/b/");
        EXPECT_EQ(deepest_ancestor_dir_with_subpath("a/b/c", "/"), "a/b/");
        EXPECT_EQ(deepest_ancestor_dir_with_subpath("a/b/c", "x"), std::nullopt);
    };
    EXPECT_EXIT(
            {
                test_impl();
                exit(0);
            },
            ::testing::ExitedWithCode(0), "");
}

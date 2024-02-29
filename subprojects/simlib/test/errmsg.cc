#include <cerrno>
#include <gtest/gtest.h>
#include <simlib/concat.hh>
#include <simlib/errmsg.hh>

// NOLINTNEXTLINE
TEST(errmsg, errmsg) {
    EXPECT_EQ(concat(" - Operation not permitted (os error ", EPERM, ')'), errmsg(EPERM));
    EXPECT_EQ(concat(" - No such file or directory (os error ", ENOENT, ')'), errmsg(ENOENT));

    errno = EPERM;
    EXPECT_EQ(concat(" - Operation not permitted (os error ", EPERM, ')'), errmsg());
    errno = ENOENT;
    EXPECT_EQ(concat(" - No such file or directory (os error ", ENOENT, ')'), errmsg());
}

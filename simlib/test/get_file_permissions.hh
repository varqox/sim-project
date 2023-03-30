#pragma once

#include <gtest/gtest.h>
#include <simlib/file_path.hh>

inline mode_t get_file_permissions(FilePath path) {
    struct stat64 st = {};
    EXPECT_EQ(stat64(path, &st), 0);
    return st.st_mode & ACCESSPERMS;
}

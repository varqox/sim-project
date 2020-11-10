#pragma once

#include "simlib/file_path.hh"

#include <gtest/gtest.h>

inline mode_t get_file_permissions(FilePath path) {
    struct stat64 st {};
    EXPECT_EQ(stat64(path, &st), 0);
    return st.st_mode & ACCESSPERMS;
}

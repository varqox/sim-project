#include <fcntl.h>
#include <gtest/gtest.h>
#include <simlib/file_descriptor.hh>
#include <simlib/file_info.hh>
#include <simlib/file_remover.hh>
#include <simlib/temporary_directory.hh>

// NOLINTNEXTLINE
TEST(FileRemover, simple) {
    TemporaryDirectory tmp_dir("/tmp/file-remover-test.XXXXXX");
    auto file_path = tmp_dir.path() + "file";
    (void)FileDescriptor{file_path, O_CREAT};
    {
        auto remover = FileRemover{file_path};
        EXPECT_TRUE(path_exists(file_path));
    }
    EXPECT_FALSE(path_exists(file_path));
}

// NOLINTNEXTLINE
TEST(FileRemover, destructed_by_exception) {
    TemporaryDirectory tmp_dir("/tmp/file-remover-test.XXXXXX");
    auto file_path = tmp_dir.path() + "file";
    (void)FileDescriptor{file_path, O_CREAT};
    try {
        auto remover = FileRemover{file_path};
        throw 0; // NOLINT
    } catch (int) { // NOLINT
    }
    EXPECT_FALSE(path_exists(file_path));
}

// NOLINTNEXTLINE
TEST(FileRemover, non_existent_path) {
    TemporaryDirectory tmp_dir("/tmp/file-remover-test.XXXXXX");
    auto file_path = tmp_dir.path() + "file";
    {
        auto remover = FileRemover{file_path};
    }
}

// NOLINTNEXTLINE
TEST(FileRemover, default_constructor) {
    TemporaryDirectory tmp_dir("/tmp/file-remover-test.XXXXXX");
    auto file_path = tmp_dir.path() + "file";
    (void)FileDescriptor{file_path, O_CREAT};
    {
        auto remover = FileRemover{};
        EXPECT_TRUE(path_exists(file_path));
    }
    EXPECT_TRUE(path_exists(file_path));
}

// NOLINTNEXTLINE
TEST(FileRemover, cancelling) {
    TemporaryDirectory tmp_dir("/tmp/file-remover-test.XXXXXX");
    auto file_path = tmp_dir.path() + "file";
    (void)FileDescriptor{file_path, O_CREAT};
    {
        auto remover = FileRemover{file_path};
        remover.cancel();
    }
    EXPECT_TRUE(path_exists(file_path));
}

// NOLINTNEXTLINE
TEST(FileRemover, move_constructor) {
    TemporaryDirectory tmp_dir("/tmp/file-remover-test.XXXXXX");
    auto file_path = tmp_dir.path() + "file1";
    (void)FileDescriptor{file_path, O_CREAT};
    {
        auto remover = FileRemover{file_path};
        {
            auto inner_remover = std::move(remover);
            EXPECT_TRUE(path_exists(file_path));
        }
        EXPECT_FALSE(path_exists(file_path));
    }
}

// NOLINTNEXTLINE
TEST(FileRemover, move_operator) {
    TemporaryDirectory tmp_dir("/tmp/file-remover-test.XXXXXX");
    auto file1_path = tmp_dir.path() + "file1";
    (void)FileDescriptor{file1_path, O_CREAT};
    auto file2_path = tmp_dir.path() + "file2";
    (void)FileDescriptor{file2_path, O_CREAT};
    {
        auto remover = FileRemover{file1_path};
        {
            auto inner_remover = FileRemover{file2_path};
            remover = std::move(inner_remover);
            EXPECT_FALSE(path_exists(file1_path));
            EXPECT_TRUE(path_exists(file2_path));
        }
        EXPECT_TRUE(path_exists(file2_path));
    }
    EXPECT_FALSE(path_exists(file2_path));
}

TEST(FileRemover, move_operator_non_existent_path) {
    TemporaryDirectory tmp_dir("/tmp/file-remover-test.XXXXXX");
    auto file_path = tmp_dir.path() + "file";
    {
        auto remover = FileRemover{file_path};
        EXPECT_NO_THROW(remover = FileRemover{});
    }
}

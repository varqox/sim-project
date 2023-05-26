#include <gtest/gtest.h>
#include <regex>
#include <simlib/ctype.hh>
#include <simlib/file_contents.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/proc_status_file.hh>
#include <simlib/string_view.hh>
#include <sys/mman.h>
#include <unistd.h>

struct Field {
    StringView name;
    StringView value;
};

Field field_name_and_value(StringView line) {
    static constexpr auto not_colon = [](char c) { return c != ':'; };
    static constexpr auto is_whitespace = [](char c) { return is_space(c); };

    Field res;
    res.name = line.extract_leading(not_colon);
    res.value = line.substring(1).without_leading(is_whitespace);
    return res;
}

// NOLINTNEXTLINE
TEST(proc_status_file, open_proc_status_and_field_from_proc_status) {
    FileDescriptor fd = open_proc_status(getpid());
    ASSERT_TRUE(fd.is_open());
    auto proc_status_contents = get_file_contents(fd);
    fd = memfd_create("proc_status_file test", MFD_CLOEXEC);
    ASSERT_TRUE(fd.is_open());
    ASSERT_EQ(write_all(fd, proc_status_contents), proc_status_contents.size());
    static constexpr auto is_newline = [](char c) { return c == '\n'; };
    static constexpr auto not_newline = [](char c) { return c != '\n'; };
    auto first_field =
        field_name_and_value(StringView{proc_status_contents}.extract_leading(not_newline));
    EXPECT_EQ(field_from_proc_status(fd, first_field.name), first_field.value)
        << "first_field.name: " << first_field.name;

    EXPECT_EQ(field_from_proc_status(fd, "Pid"), to_string(gettid()));
    EXPECT_EQ(field_from_proc_status(fd, "Tgid"), to_string(getpid()));
    EXPECT_EQ(field_from_proc_status(fd, "PPid"), to_string(getppid()));

    auto last_field = field_name_and_value(
        StringView{proc_status_contents}.without_trailing(is_newline).extract_trailing(not_newline)
    );
    EXPECT_EQ(field_from_proc_status(fd, last_field.name), last_field.value)
        << "last_field.name: " << last_field.name;
}

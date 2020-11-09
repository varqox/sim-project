#include "simlib/proc_status_file.hh"
#include "simlib/concat_tostr.hh"
#include "simlib/ctype.hh"
#include "simlib/file_contents.hh"
#include "simlib/file_descriptor.hh"
#include "simlib/string_view.hh"

#include <gtest/gtest.h>

struct Field {
	StringView name;
	StringView value;
};

Field field_name_and_value(StringView line) {
	constexpr static auto not_colon = [](char c) { return c != ':'; };
	constexpr static auto is_whitespace = [](char c) { return is_space(c); };

	Field res;
	res.name = line.extract_leading(not_colon);
	res.value = line.substring(1).without_leading(is_whitespace);
	return res;
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(proc_status_file, open_proc_status_and_field_from_proc_status) {
	FileDescriptor fd = open_proc_status(gettid());
	auto contents = get_file_contents(fd);
	EXPECT_EQ(
		contents, get_file_contents(concat("/proc/", gettid(), "/status")));

	constexpr static auto is_newline = [](char c) { return c == '\n'; };
	constexpr static auto not_newline = [](char c) { return c != '\n'; };
	auto first_field =
		field_name_and_value(StringView{contents}.extract_leading(not_newline));
	EXPECT_EQ(field_from_proc_status(fd, first_field.name), first_field.value)
		<< "first_field.name: " << first_field.name;

	EXPECT_EQ(field_from_proc_status(fd, "Pid"), to_string(gettid()));
	EXPECT_EQ(field_from_proc_status(fd, "Tgid"), to_string(getpid()));
	EXPECT_EQ(field_from_proc_status(fd, "PPid"), to_string(getppid()));

	auto last_field = field_name_and_value(StringView{contents}
											   .without_trailing(is_newline)
											   .extract_trailing(not_newline));
	EXPECT_EQ(field_from_proc_status(fd, last_field.name), last_field.value)
		<< "last_field.name: " << last_field.name;
}

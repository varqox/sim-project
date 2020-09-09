#include <gtest/gtest.h>
#include <sim/jobs.hh>

using namespace jobs; // NOLINT
using std::string;

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(jobs, append_dumped_int) {
	string buff;

	append_dumped(buff, static_cast<uint32_t>(0x1ad35af6));
	ASSERT_EQ(buff, "\x1a\xd3\x5a\xf6");

	append_dumped(buff, static_cast<uint8_t>(0x92));
	ASSERT_EQ(buff, "\x1a\xd3\x5a\xf6\x92");

	append_dumped(buff, static_cast<uint16_t>(0xb68a));
	ASSERT_EQ(buff, "\x1a\xd3\x5a\xf6\x92\xb6\x8a");

	append_dumped(buff, static_cast<uint64_t>(0x1040aab9c4aa3973));
	ASSERT_EQ(buff,
	          "\x1a\xd3\x5a\xf6\x92\xb6\x8a\x10\x40\xaa\xb9\xc4\xaa\x39\x73");
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(jobs, append_dumped_string) {
	string buff;

	append_dumped(buff, "te2i0j192jeo");
	ASSERT_EQ(buff, StringView("\0\0\0\x0cte2i0j192jeo", 16));

	append_dumped(buff, "");
	ASSERT_EQ(buff, StringView("\0\0\0\x0cte2i0j192jeo\0\0\0\0", 20));

	append_dumped(buff, "12213");
	ASSERT_EQ(buff, StringView("\0\0\0\x0cte2i0j192jeo\0\0\0\0\0\0\0\x05"
	                           "12213",
	                           29));

	append_dumped(buff, "qdsp\x03l\xffr3");
	ASSERT_EQ(buff, StringView("\0\0\0\x0cte2i0j192jeo\0\0\0\0\0\0\0\x05"
	                           "12213\0\0\0\x09qdsp\x03l\xffr3",
	                           42));
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(jobs, extract_dumped_int1) {
	StringView buff(
	   "\x1a\xd3\x5a\xf6\x92\xb6\x8a\x10\x40\xaa\xb9\xc4\xaa\x39\x73");

	ASSERT_EQ(extract_dumped_int<uint32_t>(buff), 0x1ad35af6);
	ASSERT_EQ(buff, "\x92\xb6\x8a\x10\x40\xaa\xb9\xc4\xaa\x39\x73");

	ASSERT_EQ(extract_dumped_int<uint8_t>(buff), 0x92);
	ASSERT_EQ(buff, "\xb6\x8a\x10\x40\xaa\xb9\xc4\xaa\x39\x73");

	ASSERT_EQ(extract_dumped_int<uint16_t>(buff), 0xb68a);
	ASSERT_EQ(buff, "\x10\x40\xaa\xb9\xc4\xaa\x39\x73");

	ASSERT_EQ(extract_dumped_int<uint64_t>(buff), 0x1040aab9c4aa3973);
	ASSERT_EQ(buff, "");
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(jobs, extract_dumped_int2) {
	StringView buff(
	   "\x1a\xd3\x5a\xf6\x92\xb6\x8a\x10\x40\xaa\xb9\xc4\xaa\x39\x73");

	uint32_t a = 0;
	uint8_t b = 0;
	uint16_t c = 0;
	uint64_t d = 0;

	extract_dumped(a, buff);
	ASSERT_EQ(a, 0x1ad35af6);
	ASSERT_EQ(buff, "\x92\xb6\x8a\x10\x40\xaa\xb9\xc4\xaa\x39\x73");

	extract_dumped(b, buff);
	ASSERT_EQ(b, 0x92);
	ASSERT_EQ(buff, "\xb6\x8a\x10\x40\xaa\xb9\xc4\xaa\x39\x73");

	extract_dumped(c, buff);
	ASSERT_EQ(c, 0xb68a);
	ASSERT_EQ(buff, "\x10\x40\xaa\xb9\xc4\xaa\x39\x73");

	extract_dumped(d, buff);
	ASSERT_EQ(d, 0x1040aab9c4aa3973);
	ASSERT_EQ(buff, "");
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(jobs, extract_dumped_string) {
	StringView buff("\0\0\0\x0cte2i0j192jeo\0\0\0\0\0\0\0\x05"
	                "12213\0\0\0\x09qdsp\x03l\xffr3",
	                42);

	ASSERT_EQ(extract_dumped_string(buff), "te2i0j192jeo");
	ASSERT_EQ(buff, StringView("\0\0\0\0\0\0\0\x05"
	                           "12213\0\0\0\x09qdsp\x03l\xffr3",
	                           26));

	ASSERT_EQ(extract_dumped_string(buff), "");
	ASSERT_EQ(buff, StringView("\0\0\0\x05"
	                           "12213\0\0\0\x09qdsp\x03l\xffr3",
	                           22));

	ASSERT_EQ(extract_dumped_string(buff), "12213");
	ASSERT_EQ(buff, StringView("\0\0\0\x09qdsp\x03l\xffr3", 13));

	ASSERT_EQ(extract_dumped_string(buff), "qdsp\x03l\xffr3");
	ASSERT_EQ(buff, "");
}

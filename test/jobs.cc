#include <gtest/gtest.h>
#include <sim/jobs.h>

using namespace jobs;
using std::string;

TEST (jobs, appendDumpedInt) {
	string buff;

	appendDumped(buff, (uint32_t)0x1ad35af6);
	ASSERT_EQ(buff, "\x1a\xd3\x5a\xf6");

	appendDumped(buff, (uint8_t)0x92);
	ASSERT_EQ(buff, "\x1a\xd3\x5a\xf6\x92");

	appendDumped(buff, (uint16_t)0xb68a);
	ASSERT_EQ(buff, "\x1a\xd3\x5a\xf6\x92\xb6\x8a");

	appendDumped(buff, (uint64_t)0x1040aab9c4aa3973);
	ASSERT_EQ(buff, "\x1a\xd3\x5a\xf6\x92\xb6\x8a"
		"\x10\x40\xaa\xb9\xc4\xaa\x39\x73");
}

TEST (jobs, appendDumpedString) {
	string buff;

	appendDumped(buff, "te2i0j192jeo");
	ASSERT_EQ(buff, StringView("\0\0\0\x0cte2i0j192jeo", 16));

	appendDumped(buff, "");
	ASSERT_EQ(buff, StringView("\0\0\0\x0cte2i0j192jeo\0\0\0\0", 20));

	appendDumped(buff, "12213");
	ASSERT_EQ(buff, StringView("\0\0\0\x0cte2i0j192jeo\0\0\0\0\0\0\0\x05"
		"12213", 29));

	appendDumped(buff, "qdsp\x03l\xffr3");
	ASSERT_EQ(buff, StringView("\0\0\0\x0cte2i0j192jeo\0\0\0\0\0\0\0\x05"
		"12213\0\0\0\x09qdsp\x03l\xffr3", 42));
}

TEST (jobs, extractDumpedInt1) {
	StringView buff {"\x1a\xd3\x5a\xf6\x92\xb6\x8a"
		"\x10\x40\xaa\xb9\xc4\xaa\x39\x73"};

	ASSERT_EQ(extractDumpedInt<uint32_t>(buff), 0x1ad35af6);
	ASSERT_EQ(buff, "\x92\xb6\x8a\x10\x40\xaa\xb9\xc4\xaa\x39\x73");

	ASSERT_EQ(extractDumpedInt<uint8_t>(buff), 0x92);
	ASSERT_EQ(buff, "\xb6\x8a\x10\x40\xaa\xb9\xc4\xaa\x39\x73");

	ASSERT_EQ(extractDumpedInt<uint16_t>(buff), 0xb68a);
	ASSERT_EQ(buff, "\x10\x40\xaa\xb9\xc4\xaa\x39\x73");

	ASSERT_EQ(extractDumpedInt<uint64_t>(buff), 0x1040aab9c4aa3973);
	ASSERT_EQ(buff, "");
}

TEST (jobs, extractDumpedInt2) {
	StringView buff {"\x1a\xd3\x5a\xf6\x92\xb6\x8a"
		"\x10\x40\xaa\xb9\xc4\xaa\x39\x73"};

	uint32_t a;
	uint8_t b;
	uint16_t c;
	uint64_t d;

	extractDumped(a, buff);
	ASSERT_EQ(a, 0x1ad35af6);
	ASSERT_EQ(buff, "\x92\xb6\x8a\x10\x40\xaa\xb9\xc4\xaa\x39\x73");

	extractDumped(b, buff);
	ASSERT_EQ(b, 0x92);
	ASSERT_EQ(buff, "\xb6\x8a\x10\x40\xaa\xb9\xc4\xaa\x39\x73");

	extractDumped(c, buff);
	ASSERT_EQ(c, 0xb68a);
	ASSERT_EQ(buff, "\x10\x40\xaa\xb9\xc4\xaa\x39\x73");

	extractDumped(d, buff);
	ASSERT_EQ(d, 0x1040aab9c4aa3973);
	ASSERT_EQ(buff, "");
}

TEST (jobs, extractDumpedString) {
	StringView buff {"\0\0\0\x0cte2i0j192jeo\0\0\0\0\0\0\0\x05"
		"12213\0\0\0\x09qdsp\x03l\xffr3", 42};

	ASSERT_EQ(extractDumpedString(buff), "te2i0j192jeo");
	ASSERT_EQ(buff, StringView("\0\0\0\0\0\0\0\x05"
		"12213\0\0\0\x09qdsp\x03l\xffr3", 26));

	ASSERT_EQ(extractDumpedString(buff), "");
	ASSERT_EQ(buff, StringView("\0\0\0\x05"
		"12213\0\0\0\x09qdsp\x03l\xffr3", 22));

	ASSERT_EQ(extractDumpedString(buff), "12213");
	ASSERT_EQ(buff, StringView("\0\0\0\x09qdsp\x03l\xffr3", 13));

	ASSERT_EQ(extractDumpedString(buff), "qdsp\x03l\xffr3");
	ASSERT_EQ(buff, "");
}

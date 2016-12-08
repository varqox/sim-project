#include "../include/string.h"

#include <gtest/gtest.h>

TEST (StringBuff, StringBuff) {
	EXPECT_EQ(StringBuff<102>::max_size, 101);

	EXPECT_THROW(StringBuff<10>(10, 'a'), std::runtime_error);
	EXPECT_EQ(StringBuff<10>().len, 0);
	EXPECT_EQ(StringBuff<10>("foo-bar").len, 7);
	EXPECT_EQ(StringBuff<10>(9, 'v').len, 9);
	EXPECT_EQ(StringBuff<10>(9, 'v'), "vvvvvvvvv");

	StringBuff<10> sb("aaa", 'c', StringView{"kk"});
	EXPECT_EQ(sb, "aaackk");
	sb.append('a', "bc");
	EXPECT_EQ(sb, "aaackkabc");
	EXPECT_EQ(sb.append(), "aaackkabc");
	EXPECT_THROW(sb.append('p'), std::runtime_error);
}

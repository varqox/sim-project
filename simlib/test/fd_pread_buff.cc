#include "simlib/fd_pread_buff.hh"
#include "simlib/file_contents.hh"
#include "simlib/opened_temporary_file.hh"
#include "simlib/random.hh"
#include "simlib/string_transform.hh"

#include <cstdlib>
#include <gtest/gtest.h>

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(fd_pread_buff, fd_pread_buff) {
	OpenedTemporaryFile tmp_file{"/tmp/fd_pread_buff.test.XXXXXX"};
	std::string data(1 << 14, 0);
	fill_randomly(data.data(), data.size());
	// data = to_hex(data).to_string();
	put_file_contents(tmp_file.path(), data);
	for (int iter = 0; iter < 20; ++iter) {
		int offset = get_random(0, static_cast<int>(data.size()));
		FdPreadBuff fbuff{tmp_file, offset};
		int len = get_random(0, static_cast<int>(data.size()));
		std::string str;
		for (int i = 0; i < len; ++i) {
			auto opt = fbuff.get_char();
			EXPECT_NE(opt, std::nullopt);
			if (*opt == EOF) {
				break;
			}
			str += static_cast<char>(*opt);
		}
		EXPECT_EQ(
			str,
			StringView(data).substring(
				offset, std::min<int>(offset + len, data.size())));
	}
}

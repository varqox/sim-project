#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <simlib/array_buff.hh>

// NOLINTNEXTLINE
TEST(ArrayBuff, append_bytes) {
    uint32_t x = 0xdeadbeef;
    uint16_t y = 2137;
    ArrayBuff<sizeof(x) + sizeof(y)> buff;
    buff.append_bytes(&x, sizeof(x));
    buff.append_bytes(&y, sizeof(y));
    decltype(x) xd;
    std::memcpy(&xd, buff.data(), sizeof(xd));
    decltype(y) yd;
    std::memcpy(&yd, buff.data() + sizeof(xd), sizeof(yd));
    ASSERT_EQ(x, xd);
    ASSERT_EQ(y, yd);
}

// NOLINTNEXTLINE
TEST(ArrayBuff, append_as_bytes) {
    uint32_t x = 0xdeadbeef;
    uint16_t y = 2137;
    ArrayBuff<sizeof(x) + sizeof(y)> buff;
    buff.append_as_bytes(x);
    buff.append_as_bytes(y);
    decltype(x) xd;
    std::memcpy(&xd, buff.data(), sizeof(xd));
    decltype(y) yd;
    std::memcpy(&yd, buff.data() + sizeof(xd), sizeof(yd));
    ASSERT_EQ(x, xd);
    ASSERT_EQ(y, yd);
}

// NOLINTNEXTLINE
TEST(ArrayBuff, array_buff_from) {
    uint32_t x = 0xdeadbeef;
    uint16_t y = 2137;
    auto buff = array_buff_from(x, y);
    static_assert(decltype(buff)::capacity == sizeof(x) + sizeof(y));
    decltype(x) xd;
    std::memcpy(&xd, buff.data(), sizeof(xd));
    decltype(y) yd;
    std::memcpy(&yd, buff.data() + sizeof(xd), sizeof(yd));
    ASSERT_EQ(x, xd);
    ASSERT_EQ(y, yd);
}

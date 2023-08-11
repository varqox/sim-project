#include "get_byte_of.hh"

#include <cstdint>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <optional>
#include <simlib/deserialize.hh>
#include <simlib/random.hh>

using deserialize::casted_from;
using deserialize::from;
using deserialize::Reader;
using std::byte;

// NOLINTNEXTLINE
TEST(deserialize, reader) {
    std::array<byte, 41> buff;
    fill_randomly(buff.data(), buff.size());
    auto reader = Reader{buff.data(), buff.size()};
    ASSERT_EQ(reader.extract_bytes(0), buff.data());
    ASSERT_EQ(reader.remaining_size(), buff.size());

    ASSERT_EQ(reader.extract_bytes(10), buff.data());
    ASSERT_EQ(reader.remaining_size(), buff.size() - 10);

    {
        std::array<byte, 3> tmp;
        reader.read_bytes(tmp.data(), tmp.size());
        ASSERT_EQ(tmp[0], buff[10]);
        ASSERT_EQ(tmp[1], buff[11]);
        ASSERT_EQ(tmp[2], buff[12]);
        ASSERT_EQ(reader.remaining_size(), buff.size() - 13);
    }
    {
        auto x = reader.read_bytes_as<uint8_t>();
        static_assert(std::is_same_v<decltype(x), uint8_t>);
        ASSERT_EQ(get_byte_of<0>(x), buff[13]);
        ASSERT_EQ(reader.remaining_size(), buff.size() - 14);
    }
    {
        auto x = reader.read_bytes_as<uint32_t>();
        static_assert(std::is_same_v<decltype(x), uint32_t>);
        ASSERT_EQ(get_byte_of<0>(x), buff[14]);
        ASSERT_EQ(get_byte_of<1>(x), buff[15]);
        ASSERT_EQ(get_byte_of<2>(x), buff[16]);
        ASSERT_EQ(get_byte_of<3>(x), buff[17]);
        ASSERT_EQ(reader.remaining_size(), buff.size() - 18);
    }
    {
        auto x = reader.read<int>(from<uint8_t>);
        static_assert(std::is_same_v<decltype(x), int>);
        ASSERT_EQ(x, static_cast<int>(buff[18]));
        ASSERT_EQ(reader.remaining_size(), buff.size() - 19);
    }
    {
        auto x = reader.read<int>(casted_from<uint8_t>);
        static_assert(std::is_same_v<decltype(x), int>);
        ASSERT_EQ(x, static_cast<int>(buff[19]));
        ASSERT_EQ(reader.remaining_size(), buff.size() - 20);
    }
    {
        int x;
        reader.read(x, from<uint8_t>);
        ASSERT_EQ(x, static_cast<int>(buff[20]));
        ASSERT_EQ(reader.remaining_size(), buff.size() - 21);
    }
    {
        uint32_t x;
        reader.read(x, from<uint32_t>);
        ASSERT_EQ(get_byte_of<0>(x), buff[21]);
        ASSERT_EQ(get_byte_of<1>(x), buff[22]);
        ASSERT_EQ(get_byte_of<2>(x), buff[23]);
        ASSERT_EQ(get_byte_of<3>(x), buff[24]);
        ASSERT_EQ(reader.remaining_size(), buff.size() - 25);
    }
    {
        int x;
        reader.read(x, casted_from<uint8_t>);
        ASSERT_EQ(x, static_cast<int>(buff[25]));
        ASSERT_EQ(reader.remaining_size(), buff.size() - 26);
    }
    {
        uint32_t x;
        reader.read(x, casted_from<uint32_t>);
        ASSERT_EQ(get_byte_of<0>(x), buff[26]);
        ASSERT_EQ(get_byte_of<1>(x), buff[27]);
        ASSERT_EQ(get_byte_of<2>(x), buff[28]);
        ASSERT_EQ(get_byte_of<3>(x), buff[29]);
        ASSERT_EQ(reader.remaining_size(), buff.size() - 30);
    }
    {
        bool b[8];
        for (auto& bx : b) {
            bx = get_random(0, 1);
        }
        reader.read_flags({
            {b[0], 1 << 0},
            {b[1], 1 << 1},
            {b[2], 1 << 2},
            {b[3], 1 << 3},
            {b[4], 1 << 4},
            {b[5], 1 << 5},
            {b[6], 1 << 6},
            {b[7], 1 << 7},
        }, from<uint8_t>);
        int x = b[0] | (b[1] << 1) | (b[2] << 2) | (b[3] << 3) | (b[4] << 4) | (b[5] << 5) |
            (b[6] << 6) | (b[7] << 7);
        ASSERT_EQ(x, static_cast<int>(buff[30]));
        ASSERT_EQ(reader.remaining_size(), buff.size() - 31);
    }
    {
        std::optional<int> opt = 42;
        reader.read_optional_if(opt, from<uint8_t>, false);
        ASSERT_FALSE(opt.has_value());
        ASSERT_EQ(reader.remaining_size(), buff.size() - 31);

        reader.read_optional_if(opt, from<uint8_t>, true);
        ASSERT_TRUE(opt.has_value());
        ASSERT_EQ(*opt, static_cast<int>(buff[31])); // NOLINT(bugprone-unchecked-optional-access)
        ASSERT_EQ(reader.remaining_size(), buff.size() - 32);
    }
    {
        std::optional<int32_t> opt = 42;
        reader.read_optional_if(opt, from<int32_t>, false);
        ASSERT_FALSE(opt.has_value());
        ASSERT_EQ(reader.remaining_size(), buff.size() - 32);

        reader.read_optional_if(opt, from<int32_t>, true);
        ASSERT_TRUE(opt.has_value());
        ASSERT_EQ(get_byte_of<0>(*opt), buff[32]); // NOLINT(bugprone-unchecked-optional-access)
        ASSERT_EQ(get_byte_of<1>(*opt), buff[33]); // NOLINT(bugprone-unchecked-optional-access)
        ASSERT_EQ(get_byte_of<2>(*opt), buff[34]); // NOLINT(bugprone-unchecked-optional-access)
        ASSERT_EQ(get_byte_of<3>(*opt), buff[35]); // NOLINT(bugprone-unchecked-optional-access)
        ASSERT_EQ(reader.remaining_size(), buff.size() - 36);
    }
    {
        std::optional<int> opt = 42;
        reader.read_optional_if(opt, casted_from<uint8_t>, false);
        ASSERT_FALSE(opt.has_value());
        ASSERT_EQ(reader.remaining_size(), buff.size() - 36);

        reader.read_optional_if(opt, casted_from<uint8_t>, true);
        ASSERT_TRUE(opt.has_value());
        ASSERT_EQ(*opt, static_cast<int>(buff[36])); // NOLINT(bugprone-unchecked-optional-access)
        ASSERT_EQ(reader.remaining_size(), buff.size() - 37);
    }
    {
        std::optional<int32_t> opt = 42;
        reader.read_optional_if(opt, casted_from<int32_t>, false);
        ASSERT_FALSE(opt.has_value());
        ASSERT_EQ(reader.remaining_size(), buff.size() - 37);

        reader.read_optional_if(opt, casted_from<int32_t>, true);
        ASSERT_TRUE(opt.has_value());
        ASSERT_EQ(get_byte_of<0>(*opt), buff[37]); // NOLINT(bugprone-unchecked-optional-access)
        ASSERT_EQ(get_byte_of<1>(*opt), buff[38]); // NOLINT(bugprone-unchecked-optional-access)
        ASSERT_EQ(get_byte_of<2>(*opt), buff[39]); // NOLINT(bugprone-unchecked-optional-access)
        ASSERT_EQ(get_byte_of<3>(*opt), buff[40]); // NOLINT(bugprone-unchecked-optional-access)
        ASSERT_EQ(reader.remaining_size(), buff.size() - 41);
    }

    ASSERT_THAT(
        [&] { reader.extract_bytes(1); },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("cannot read 1 bytes, have only 0 bytes")
        )
    );
    ASSERT_THAT(
        ([&] {
            std::array<byte, 3> tmp;
            reader.read_bytes(tmp.data(), tmp.size());
        }),
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("cannot read 3 bytes, have only 0 bytes")
        )
    );
    ASSERT_THAT(
        [&] { (void)reader.read_bytes_as<uint8_t>(); },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("cannot read 1 bytes, have only 0 bytes")
        )
    );
    ASSERT_THAT(
        [&] { (void)reader.read<int>(from<uint8_t>); },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("cannot read 1 bytes, have only 0 bytes")
        )
    );
    ASSERT_THAT(
        [&] { (void)reader.read<int>(casted_from<uint8_t>); },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("cannot read 1 bytes, have only 0 bytes")
        )
    );
    ASSERT_THAT(
        [&] {
            int x;
            reader.read(x, from<uint8_t>);
        },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("cannot read 1 bytes, have only 0 bytes")
        )
    );
    ASSERT_THAT(
        [&] {
            int x;
            reader.read(x, casted_from<uint8_t>);
        },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("cannot read 1 bytes, have only 0 bytes")
        )
    );
    ASSERT_THAT(
        [&] {
            bool b;
            reader.read_flags({{b, 1}}, from<uint8_t>);
        },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("cannot read 1 bytes, have only 0 bytes")
        )
    );
    ASSERT_THAT(
        [&] {
            std::optional<int> x;
            reader.read_optional_if(x, from<int8_t>, true);
        },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("cannot read 1 bytes, have only 0 bytes")
        )
    );
    ASSERT_THAT(
        [&] {
            std::optional<int> x;
            reader.read_optional_if(x, casted_from<int8_t>, true);
        },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("cannot read 1 bytes, have only 0 bytes")
        )
    );
    ASSERT_EQ(reader.remaining_size(), 0);
}

// NOLINTNEXTLINE
TEST(deserialize, casted_from_conversion_errors) {
    std::array<byte, 40> buff;
    for (auto& b : buff) {
        b = byte{0xaa};
    }
    auto reader = Reader{buff.data(), buff.size()};

    ASSERT_THAT(
        [&] { (void)reader.read<uint8_t>(casted_from<uint32_t>); },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("Cannot safely convert value: 2863311530 to h")
        )
    );
    ASSERT_EQ(reader.remaining_size(), buff.size());

    ASSERT_THAT(
        [&] {
            uint8_t x;
            reader.read(x, casted_from<uint32_t>);
        },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("Cannot safely convert value: 2863311530 to h")
        )
    );
    ASSERT_EQ(reader.remaining_size(), buff.size());

    ASSERT_THAT(
        [&] {
            std::optional<int8_t> opt;
            reader.read_optional_if(opt, casted_from<uint32_t>, true);
        },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("Cannot safely convert value: 2863311530 to a")
        )
    );
    ASSERT_EQ(reader.remaining_size(), buff.size());
}

// NOLINTNEXTLINE
TEST(deserialize, read_flags_errors) {
    auto b = byte{1 | 4 | 16 | 64};
    auto reader = Reader{&b, 1};
    bool b1;
    bool b8;
    bool b16;
    ASSERT_THAT(
        [&] {
            reader.read_flags({
                {b1, 1},
                {b8, 8},
                {b16, 16},
            }, from<uint8_t>);
        },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("found unexpected flags bits: (dec) 68")
        )
    );
    ASSERT_EQ(b1, true);
    ASSERT_EQ(b8, false);
    ASSERT_EQ(b16, true);
    ASSERT_EQ(reader.remaining_size(), 0);
}

// NOLINTNEXTLINE
TEST(deserialize, find_byte) {
    auto buff = std::array{byte{1}, byte{2}, byte{48}, byte{2}, byte{1}};
    auto reader = Reader{buff.data(), buff.size()};
    ASSERT_EQ(reader.find_byte(byte{1}), 0);
    ASSERT_EQ(reader.find_byte(byte{2}), 1);
    ASSERT_EQ(reader.find_byte(byte{48}), 2);
    ASSERT_EQ(reader.find_byte(byte{42}), std::nullopt);
    reader.extract_bytes(2);
    ASSERT_EQ(reader.find_byte(byte{1}), 2);
    ASSERT_EQ(reader.find_byte(byte{2}), 1);
    ASSERT_EQ(reader.find_byte(byte{48}), 0);
    ASSERT_EQ(reader.find_byte(byte{42}), std::nullopt);
    reader.extract_bytes(1);
    ASSERT_EQ(reader.find_byte(byte{1}), 1);
    ASSERT_EQ(reader.find_byte(byte{2}), 0);
    ASSERT_EQ(reader.find_byte(byte{48}), std::nullopt);
    reader.extract_bytes(2);
    ASSERT_EQ(reader.find_byte(byte{1}), std::nullopt);
    ASSERT_EQ(reader.find_byte(byte{2}), std::nullopt);
    ASSERT_EQ(reader.find_byte(byte{48}), std::nullopt);
}

#include <cstdint>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <simlib/serialize.hh>
#include <stdexcept>

using serialize::as;
using serialize::casted_as;
using serialize::Phase;
using serialize::Writer;
using std::byte;

// NOLINTNEXTLINE
TEST(serialize, writer_count_len) {
    Writer<Phase::CountLen> count_writer;
    ASSERT_EQ(count_writer.written_bytes_num(), 0);

    count_writer.write_bytes("abc", 3);
    ASSERT_EQ(count_writer.written_bytes_num(), 3);

    count_writer.write_bytes("", 1);
    ASSERT_EQ(count_writer.written_bytes_num(), 4);

    count_writer.write_as_bytes(uint8_t{0xdd});
    ASSERT_EQ(count_writer.written_bytes_num(), 5);

    count_writer.write_as_bytes(uint32_t{0x0a1b2c3d});
    ASSERT_EQ(count_writer.written_bytes_num(), 9);

    count_writer.write(true, as<uint8_t>);
    ASSERT_EQ(count_writer.written_bytes_num(), 10);

    count_writer.write(uint32_t{0x01234567}, as<uint32_t>);
    ASSERT_EQ(count_writer.written_bytes_num(), 14);

    count_writer.write(-42, casted_as<int8_t>);
    ASSERT_EQ(count_writer.written_bytes_num(), 15);

    count_writer.write(0x01234567, casted_as<uint32_t>);
    ASSERT_EQ(count_writer.written_bytes_num(), 19);

    ASSERT_THAT(
        [&] { count_writer.write(256, casted_as<uint8_t>); },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("cannot safely convert value: 256 to h")
        )
    );
    ASSERT_THAT(
        [&] { count_writer.write(-1, casted_as<uint8_t>); },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("cannot safely convert value: -1 to h")
        )
    );
    ASSERT_THAT(
        [&] { count_writer.write(19'088'743, casted_as<int16_t>); },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("cannot safely convert value: 19088743 to s")
        )
    );
    ASSERT_EQ(count_writer.written_bytes_num(), 19);

    count_writer.write_flags({
        {true, 1},
        {false, 16},
        {true, 128},
    }, as<uint8_t>);
    ASSERT_EQ(count_writer.written_bytes_num(), 20);

    count_writer.write_flags({
        {true, 32},
        {false, 128},
        {true, 1024},
    }, as<uint16_t>);
    ASSERT_EQ(count_writer.written_bytes_num(), 22);
}

template <size_t N, class T>
byte get_byte_of(const T& val) noexcept {
    static_assert(N < sizeof(T)); // NOLINT(bugprone-sizeof-expression)
    byte res;
    std::memcpy(&res, reinterpret_cast<const byte*>(&val) + N, 1);
    return res;
}

// NOLINTNEXTLINE
TEST(serialize, writer) {
    auto buff = std::array<byte, 22>{};
    auto expected_buff = std::array<byte, 22>{};
    auto writer = Writer<Phase::Serialize>{buff.data(), buff.size()};
    ASSERT_EQ(writer.remaining_size(), buff.size() - 0);

    writer.write_bytes("abc", 3);
    ASSERT_EQ(writer.remaining_size(), buff.size() - 3);
    expected_buff[0] = byte{'a'};
    expected_buff[1] = byte{'b'};
    expected_buff[2] = byte{'c'};
    ASSERT_EQ(buff, expected_buff);

    writer.write_bytes("", 1);
    ASSERT_EQ(writer.remaining_size(), buff.size() - 4);
    expected_buff[3] = byte{0};
    ASSERT_EQ(buff, expected_buff);

    writer.write_as_bytes(uint8_t{0xdd});
    ASSERT_EQ(writer.remaining_size(), buff.size() - 5);
    expected_buff[4] = byte{0xdd};
    ASSERT_EQ(buff, expected_buff);

    writer.write_as_bytes(uint32_t{0x0a1b2c3d});
    ASSERT_EQ(writer.remaining_size(), buff.size() - 9);
    expected_buff[5] = get_byte_of<0>(uint32_t{0x0a1b2c3d});
    expected_buff[6] = get_byte_of<1>(uint32_t{0x0a1b2c3d});
    expected_buff[7] = get_byte_of<2>(uint32_t{0x0a1b2c3d});
    expected_buff[8] = get_byte_of<3>(uint32_t{0x0a1b2c3d});
    ASSERT_EQ(buff, expected_buff);

    writer.write(true, as<uint8_t>);
    ASSERT_EQ(writer.remaining_size(), buff.size() - 10);
    expected_buff[9] = byte{1};
    ASSERT_EQ(buff, expected_buff);

    writer.write(uint32_t{0x01234567}, as<uint32_t>);
    ASSERT_EQ(writer.remaining_size(), buff.size() - 14);
    expected_buff[10] = get_byte_of<0>(uint32_t{0x1234567});
    expected_buff[11] = get_byte_of<1>(uint32_t{0x1234567});
    expected_buff[12] = get_byte_of<2>(uint32_t{0x1234567});
    expected_buff[13] = get_byte_of<3>(uint32_t{0x1234567});
    ASSERT_EQ(buff, expected_buff);

    writer.write(-42, casted_as<int8_t>);
    ASSERT_EQ(writer.remaining_size(), buff.size() - 15);
    expected_buff[14] = get_byte_of<0>(int8_t{-42});
    ASSERT_EQ(buff, expected_buff);

    writer.write(0x01234567, casted_as<uint32_t>);
    ASSERT_EQ(writer.remaining_size(), buff.size() - 19);
    expected_buff[15] = get_byte_of<0>(uint32_t{0x1234567});
    expected_buff[16] = get_byte_of<1>(uint32_t{0x1234567});
    expected_buff[17] = get_byte_of<2>(uint32_t{0x1234567});
    expected_buff[18] = get_byte_of<3>(uint32_t{0x1234567});
    ASSERT_EQ(buff, expected_buff);

    ASSERT_THAT(
        [&] { writer.write(256, casted_as<uint8_t>); },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("cannot safely convert value: 256 to h")
        )
    );
    ASSERT_THAT(
        [&] { writer.write(-1, casted_as<uint8_t>); },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("cannot safely convert value: -1 to h")
        )
    );
    ASSERT_THAT(
        [&] { writer.write(19'088'743, casted_as<int16_t>); },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("cannot safely convert value: 19088743 to s")
        )
    );
    ASSERT_EQ(writer.remaining_size(), buff.size() - 19);
    ASSERT_EQ(buff, expected_buff);

    writer.write_flags({
        {true, 1},
        {false, 16},
        {true, 128},
    }, as<uint8_t>);
    ASSERT_EQ(writer.remaining_size(), buff.size() - 20);
    expected_buff[19] = byte{1 | 128};
    ASSERT_EQ(buff, expected_buff);

    writer.write_flags({
        {true, 32},
        {false, 128},
        {true, 1024},
    }, as<uint16_t>);
    ASSERT_EQ(writer.remaining_size(), buff.size() - 22);
    expected_buff[20] = get_byte_of<0>(uint32_t{32 | 1024});
    expected_buff[21] = get_byte_of<1>(uint32_t{32 | 1024});
    ASSERT_EQ(buff, expected_buff);

    ASSERT_THAT(
        [&] { writer.write_bytes("abc", 3); },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("cannot write 3 bytes, have space only for 0 bytes")
        )
    );
    ASSERT_THAT(
        [&] { writer.write_as_bytes(uint8_t{42}); },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("cannot write 1 bytes, have space only for 0 bytes")
        )
    );
    ASSERT_THAT(
        [&] { writer.write(uint8_t{42}, as<uint8_t>); },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("cannot write 1 bytes, have space only for 0 bytes")
        )
    );
    ASSERT_THAT(
        [&] { writer.write(42, casted_as<uint8_t>); },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("cannot write 1 bytes, have space only for 0 bytes")
        )
    );
    ASSERT_THAT(
        [&] {
            writer.write_flags({{true, 1}}, as<uint8_t>);
        },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("cannot write 1 bytes, have space only for 0 bytes")
        )
    );
    ASSERT_EQ(writer.remaining_size(), 0);
}

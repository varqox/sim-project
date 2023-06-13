#include <cstdint>
#include <gtest/gtest.h>
#include <simlib/array_buff.hh>
#include <simlib/deserializer.hh>
#include <simlib/random_bytes.hh>

// NOLINTNEXTLINE
TEST(Deserializer, constructor_and_is_empty_and_size_and_data) {
    auto buff = array_buff_from(static_cast<uint8_t>(0x42), static_cast<uint32_t>(0xdeadbeef));
    Deserializer d{buff.data(), buff.size()};
    static_assert(std::is_same_v<decltype(d.data()), const std::byte*>);
    ASSERT_EQ(d.data(), buff.data());
    ASSERT_EQ(d.is_empty(), false);
    ASSERT_EQ(d.size(), 5);
    uint8_t x;
    d.deserialize_bytes_as(x);
    ASSERT_EQ(d.data(), buff.data() + 1);
    ASSERT_EQ(d.is_empty(), false);
    ASSERT_EQ(d.size(), 4);
    uint32_t y;
    d.deserialize_bytes_as(y);
    ASSERT_EQ(d.data(), buff.data() + 5);
    ASSERT_EQ(d.is_empty(), true);
    ASSERT_EQ(d.size(), 0);
}

// NOLINTNEXTLINE
TEST(Deserializer, operator_index) {
    for (int iter = 0; iter < 10; ++iter) {
        auto buff = random_bytes(17);
        Deserializer d{buff.data(), buff.size()};
        if (!buff.empty()) {
            for (int index_iter = 0; index_iter < 10; ++index_iter) {
                auto i = get_random<size_t>(0, buff.size() - 1);
                static_assert(std::is_same_v<decltype(d[i]), const std::byte&>);
                EXPECT_EQ(d[i], static_cast<std::byte>(buff[i]));
            }
        }
    }
}

// NOLINTNEXTLINE
TEST(Deserializer_DeathTest, operator_index_out_of_range) {
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            char buff[1] = {};
            (void)Deserializer{buff, 0}[0];
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            char buff[1] = {};
            (void)Deserializer{buff, sizeof(buff)}[1];
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            char buff[17] = {};
            (void)Deserializer{buff, sizeof(buff)}[17];
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            char buff[17] = {};
            (void)Deserializer{buff, sizeof(buff)}[1642];
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
}

// NOLINTNEXTLINE
TEST(Deserializer, deserialize_bytes) {
    auto buff = array_buff_from(static_cast<uint8_t>(0x42), static_cast<uint32_t>(0xdeadbeef));
    uint8_t x;
    uint32_t y;
    Deserializer{buff.data(), buff.size()}
        .deserialize_bytes(&x, sizeof(x))
        .deserialize_bytes(&y, sizeof(y));
    ASSERT_EQ(x, 0x42);
    ASSERT_EQ(y, 0xdeadbeef);
}

// NOLINTNEXTLINE
TEST(Deserializer, deserialize_bytes_as) {
    auto buff = array_buff_from(static_cast<uint8_t>(0x42), static_cast<uint32_t>(0xdeadbeef));
    uint8_t x;
    uint32_t y;
    Deserializer{buff.data(), buff.size()}.deserialize_bytes_as(x).deserialize_bytes_as(y);
    ASSERT_EQ(x, 0x42);
    ASSERT_EQ(y, 0xdeadbeef);
}

// NOLINTNEXTLINE
TEST(Deserializer, extract_bytes) {
    for (int iter = 0; iter < 10; ++iter) {
        auto buff = random_bytes(17);
        Deserializer d{buff.data(), buff.size()};
        auto i = get_random<size_t>(0, buff.size());
        auto d2 = d.extract_bytes(i);
        static_assert(std::is_same_v<decltype(d2), Deserializer>);
        ASSERT_EQ(d.data(), static_cast<void*>(buff.data() + i));
        ASSERT_EQ(d.size(), buff.size() - i);
        ASSERT_EQ(d2.data(), static_cast<void*>(buff.data()));
        ASSERT_EQ(d2.size(), i);
    }
}

// NOLINTNEXTLINE
TEST(MutDeserializer, constructor_and_is_empty_and_size_and_data) {
    auto buff = array_buff_from(static_cast<uint8_t>(0x42), static_cast<uint32_t>(0xdeadbeef));
    MutDeserializer d{buff.data(), buff.size()};
    static_assert(std::is_same_v<decltype(d.data()), std::byte*>);
    ASSERT_EQ(d.data(), buff.data());
    ASSERT_EQ(d.is_empty(), false);
    ASSERT_EQ(d.size(), 5);
    uint8_t x;
    d.deserialize_bytes_as(x);
    ASSERT_EQ(d.data(), buff.data() + 1);
    ASSERT_EQ(d.is_empty(), false);
    ASSERT_EQ(d.size(), 4);
    uint32_t y;
    d.deserialize_bytes_as(y);
    ASSERT_EQ(d.data(), buff.data() + 5);
    ASSERT_EQ(d.is_empty(), true);
    ASSERT_EQ(d.size(), 0);
}

// NOLINTNEXTLINE
TEST(MutDeserializer, operator_index) {
    for (int iter = 0; iter < 10; ++iter) {
        auto buff = random_bytes(17);
        MutDeserializer d{buff.data(), buff.size()};
        if (!buff.empty()) {
            for (int index_iter = 0; index_iter < 10; ++index_iter) {
                auto i = get_random<size_t>(0, buff.size() - 1);
                static_assert(std::is_same_v<decltype(d[i]), std::byte&>);
                EXPECT_EQ(d[i], static_cast<std::byte>(buff[i]));
            }
        }
    }
}

// NOLINTNEXTLINE
TEST(MutDeserializer_DeathTest, operator_index_out_of_range) {
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            char buff[1];
            (void)MutDeserializer{buff, 0}[0];
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            char buff[1];
            (void)MutDeserializer{buff, sizeof(buff)}[1];
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            char buff[17];
            (void)MutDeserializer{buff, sizeof(buff)}[17];
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
    // NOLINTNEXTLINE
    EXPECT_EXIT(
        ([] {
            char buff[17];
            (void)MutDeserializer{buff, sizeof(buff)}[1642];
        }()),
        testing::KilledBySignal(SIGABRT),
        ""
    );
}

// NOLINTNEXTLINE
TEST(MutDeserializer, deserialize_bytes) {
    auto buff = array_buff_from(static_cast<uint8_t>(0x42), static_cast<uint32_t>(0xdeadbeef));
    uint8_t x;
    uint32_t y;
    MutDeserializer{buff.data(), buff.size()}
        .deserialize_bytes(&x, sizeof(x))
        .deserialize_bytes(&y, sizeof(y));
    ASSERT_EQ(x, 0x42);
    ASSERT_EQ(y, 0xdeadbeef);
}

// NOLINTNEXTLINE
TEST(MutDeserializer, deserialize_bytes_as) {
    auto buff = array_buff_from(static_cast<uint8_t>(0x42), static_cast<uint32_t>(0xdeadbeef));
    uint8_t x;
    uint32_t y;
    MutDeserializer{buff.data(), buff.size()}.deserialize_bytes_as(x).deserialize_bytes_as(y);
    ASSERT_EQ(x, 0x42);
    ASSERT_EQ(y, 0xdeadbeef);
}

// NOLINTNEXTLINE
TEST(MutDeserializer, extract_bytes) {
    for (int iter = 0; iter < 10; ++iter) {
        auto buff = random_bytes(17);
        MutDeserializer d{buff.data(), buff.size()};
        auto i = get_random<size_t>(0, buff.size());
        auto d2 = d.extract_bytes(i);
        static_assert(std::is_same_v<decltype(d2), MutDeserializer>);
        ASSERT_EQ(d.data(), static_cast<void*>(buff.data() + i));
        ASSERT_EQ(d.size(), buff.size() - i);
        ASSERT_EQ(d2.data(), static_cast<void*>(buff.data()));
        ASSERT_EQ(d2.size(), i);
    }
}

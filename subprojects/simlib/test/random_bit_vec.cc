#include <gtest/gtest.h>
#include <simlib/random.hh>
#include <simlib/random_bit_vector.hh>

// NOLINTNEXTLINE
TEST(RandomBitVector, works) {
    constexpr int REPS = 100;
    for (size_t rep = 0; rep < REPS; ++rep) {
        size_t len = get_random(0, 64);
        auto v = RandomBitVector{len};
        EXPECT_EQ(v.size(), len);
        for (size_t i = 0; i < len; ++i) {
            bool val = get_random(0, 1);
            v.set(i, val);
            EXPECT_EQ(v.get(i), val);
        }
    }
}

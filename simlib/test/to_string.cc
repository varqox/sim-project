#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <simlib/to_string.hh>

// NOLINTNEXTLINE
TEST(to_string, to_string_with_integral) {
    static_assert(to_string(false) == "false");
    static_assert(to_string(true) == "true");

    static_assert(to_string(static_cast<char>(48)) == "0");
    // NOLINTNEXTLINE(bugprone-string-literal-with-embedded-nul)
    static_assert(to_string(static_cast<char>(0)) == "\0");

    static_assert(to_string(std::numeric_limits<int8_t>::min()) == "-128");
    static_assert(to_string(static_cast<int8_t>(-107)) == "-107");
    static_assert(to_string(static_cast<int8_t>(-17)) == "-17");
    static_assert(to_string(static_cast<int8_t>(-1)) == "-1");
    static_assert(to_string(static_cast<int8_t>(0)) == "0");
    static_assert(to_string(static_cast<int8_t>(1)) == "1");
    static_assert(to_string(static_cast<int8_t>(42)) == "42");
    static_assert(to_string(static_cast<int8_t>(118)) == "118");
    static_assert(to_string(std::numeric_limits<int8_t>::max()) == "127");
    static_assert(to_string(std::numeric_limits<uint8_t>::min()) == "0");
    static_assert(to_string(static_cast<uint8_t>(0)) == "0");
    static_assert(to_string(static_cast<uint8_t>(1)) == "1");
    static_assert(to_string(static_cast<uint8_t>(42)) == "42");
    static_assert(to_string(static_cast<uint8_t>(239)) == "239");
    static_assert(to_string(std::numeric_limits<uint8_t>::max()) == "255");

    static_assert(to_string(std::numeric_limits<int16_t>::min()) == "-32768");
    static_assert(to_string(static_cast<int16_t>(-26358)) == "-26358");
    static_assert(to_string(static_cast<int16_t>(-17)) == "-17");
    static_assert(to_string(static_cast<int16_t>(-1)) == "-1");
    static_assert(to_string(static_cast<int16_t>(0)) == "0");
    static_assert(to_string(static_cast<int16_t>(1)) == "1");
    static_assert(to_string(static_cast<int16_t>(42)) == "42");
    static_assert(to_string(static_cast<int16_t>(16356)) == "16356");
    static_assert(to_string(std::numeric_limits<int16_t>::max()) == "32767");
    static_assert(to_string(std::numeric_limits<uint16_t>::min()) == "0");
    static_assert(to_string(static_cast<uint16_t>(0)) == "0");
    static_assert(to_string(static_cast<uint16_t>(1)) == "1");
    static_assert(to_string(static_cast<uint16_t>(42)) == "42");
    static_assert(to_string(static_cast<uint16_t>(53621)) == "53621");
    static_assert(to_string(std::numeric_limits<uint16_t>::max()) == "65535");

    static_assert(to_string(std::numeric_limits<int32_t>::min()) == "-2147483648");
    static_assert(to_string(static_cast<int32_t>(-1'642'516'759)) == "-1642516759");
    static_assert(to_string(static_cast<int32_t>(-17)) == "-17");
    static_assert(to_string(static_cast<int32_t>(-1)) == "-1");
    static_assert(to_string(static_cast<int32_t>(0)) == "0");
    static_assert(to_string(static_cast<int32_t>(1)) == "1");
    static_assert(to_string(static_cast<int32_t>(42)) == "42");
    static_assert(to_string(static_cast<int32_t>(2'136'532'621)) == "2136532621");
    static_assert(to_string(std::numeric_limits<int32_t>::max()) == "2147483647");
    static_assert(to_string(std::numeric_limits<uint32_t>::min()) == "0");
    static_assert(to_string(static_cast<uint32_t>(0)) == "0");
    static_assert(to_string(static_cast<uint32_t>(1)) == "1");
    static_assert(to_string(static_cast<uint32_t>(42)) == "42");
    static_assert(to_string(static_cast<uint32_t>(3'643'551'363)) == "3643551363");
    static_assert(to_string(std::numeric_limits<uint32_t>::max()) == "4294967295");

    static_assert(to_string(std::numeric_limits<int64_t>::min()) == "-9223372036854775808");
    static_assert(
        to_string(static_cast<int64_t>(-6'452'185'143'265'312'634)) == "-6452185143265312634"
    );
    static_assert(to_string(static_cast<int64_t>(-17)) == "-17");
    static_assert(to_string(static_cast<int64_t>(-1)) == "-1");
    static_assert(to_string(static_cast<int64_t>(0)) == "0");
    static_assert(to_string(static_cast<int64_t>(1)) == "1");
    static_assert(to_string(static_cast<int64_t>(42)) == "42");
    static_assert(
        to_string(static_cast<int64_t>(6'421'358'215'683'281'653)) == "6421358215683281653"
    );
    static_assert(to_string(std::numeric_limits<int64_t>::max()) == "9223372036854775807");
    static_assert(to_string(std::numeric_limits<uint64_t>::min()) == "0");
    static_assert(to_string(static_cast<uint64_t>(0)) == "0");
    static_assert(to_string(static_cast<uint64_t>(1)) == "1");
    static_assert(to_string(static_cast<uint64_t>(42)) == "42");
    static_assert(
        to_string(static_cast<uint64_t>(16'421'358'215'683'281'653ULL)) == "16421358215683281653"
    );
    static_assert(to_string(std::numeric_limits<uint64_t>::max()) == "18446744073709551615");
}

// NOLINTNEXTLINE
TEST(DISABLED_to_string, to_string_with_char) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_to_string, to_string_with_bool) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_to_string, to_string_with_floating_point) {
    // TODO: implement it
}

// NOLINTNEXTLINE
TEST(DISABLED_to_string, to_string_with_floating_point_with_precision) {
    // TODO: implement it
}

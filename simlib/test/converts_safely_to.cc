#include <cstdint>
#include <limits>
#include <simlib/converts_safely_to.hh>

int main() {
    // To narrower type
    static_assert(converts_safely_to<uint8_t>(255));
    static_assert(!converts_safely_to<uint8_t>(256));

    static_assert(converts_safely_to<int8_t>(intmax_t{127}));
    static_assert(!converts_safely_to<int8_t>(intmax_t{128}));
    static_assert(converts_safely_to<int8_t>(intmax_t{-128}));
    static_assert(!converts_safely_to<int8_t>(intmax_t{-129}));

    static_assert(converts_safely_to<uint8_t>(uintmax_t{255}));
    static_assert(!converts_safely_to<uint8_t>(uintmax_t{256}));
    static_assert(converts_safely_to<uint8_t>(uintmax_t{0}));

    static_assert(!converts_safely_to<uint8_t>(-1));
    static_assert(!converts_safely_to<uint32_t>(-1));
    static_assert(!converts_safely_to<uintmax_t>(-1));
    static_assert(!converts_safely_to<uintmax_t>(intmax_t{-1}));

    static_assert(!converts_safely_to<uint8_t>(intmax_t{-127}));
    static_assert(!converts_safely_to<uint8_t>(intmax_t{-128}));
    static_assert(!converts_safely_to<uint8_t>(intmax_t{-129}));

    static_assert(converts_safely_to<uint8_t>(intmax_t{255}));
    static_assert(!converts_safely_to<uint8_t>(intmax_t{256}));
    static_assert(!converts_safely_to<uint8_t>(std::numeric_limits<intmax_t>::max()));

    static_assert(converts_safely_to<int8_t>(uintmax_t{127}));
    static_assert(converts_safely_to<int8_t>(uintmax_t{127}));
    static_assert(converts_safely_to<intmax_t>(uintmax_t{std::numeric_limits<intmax_t>::max()}));
    static_assert(!converts_safely_to<intmax_t>(uintmax_t{std::numeric_limits<intmax_t>::max()} + 1)
    );
    // To larger type
    static_assert(converts_safely_to<uint16_t>(uint8_t{255}));
    static_assert(converts_safely_to<uint16_t>(uint8_t{0}));

    static_assert(converts_safely_to<int16_t>(int8_t{127}));
    static_assert(converts_safely_to<int16_t>(int8_t{-128}));

    static_assert(converts_safely_to<uint16_t>(int8_t{127}));
    static_assert(converts_safely_to<uint16_t>(int8_t{0}));

    static_assert(converts_safely_to<int16_t>(uint8_t{255}));
    static_assert(converts_safely_to<int16_t>(uint8_t{0}));
}

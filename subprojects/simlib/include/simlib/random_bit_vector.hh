#pragma once

#include <simlib/random.hh>

#include <cstddef>
#include <cstdint>
#include <memory>

class RandomBitVector {
public:
    explicit RandomBitVector(size_t bits)
    : data{std::make_unique<uint8_t[]>((bits + 7) >> 3)}
    , size_{bits} {
        fill_randomly(data.get(), (bits + 7) >> 3);
    }

    [[nodiscard]] size_t size() const noexcept { return size_; }

    [[nodiscard]] bool get(size_t bit_idx) const noexcept {
        return data.get()[bit_idx >> 3] & (1 << (bit_idx & 7));
    }

    void set(size_t bit_idx, bool value) noexcept {
        // Zeroize bit
        data.get()[bit_idx >> 3] &= ~static_cast<uint8_t>(uint8_t{1} << (bit_idx & 7));
        // Set bit
        uint8_t bit_val = value ? uint8_t{1} : uint8_t{0};
        data.get()[bit_idx >> 3] |= static_cast<uint8_t>(bit_val << (bit_idx & 7));
    }

private:
    std::unique_ptr<uint8_t[]> data;
    size_t size_;
};

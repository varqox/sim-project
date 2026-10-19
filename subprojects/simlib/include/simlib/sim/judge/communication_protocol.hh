#pragma once

#include <array>
#include <cstdint>

namespace sim::judge::communication_protocol {

constexpr uint32_t VERSION = 1;
constexpr uint32_t MAX_PAYLOAD_SIZE = 64 << 10;

enum class Command : uint32_t {
    Hello = 1,
    PrepareBatch,
    BatchInput,
    CommitBatch,
    Start,
    Input,
    CloseInput,
    Finish,
};

enum class Event : uint32_t {
    Hello = 101,
    Started,
    Output,
    OutputClosed,
    Ended,
};

inline uint32_t decode(const char* bytes) noexcept {
    uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) {
        value |= uint32_t{static_cast<unsigned char>(bytes[i])} << (8 * i);
    }
    return value;
}

inline void encode(char* bytes, uint32_t value) noexcept {
    for (unsigned i = 0; i < 4; ++i) {
        bytes[i] = static_cast<char>(value >> (8 * i));
    }
}

inline std::array<char, 12> header(uint32_t type, uint32_t id, uint32_t size) noexcept {
    std::array<char, 12> bytes;
    encode(bytes.data(), type);
    encode(bytes.data() + 4, id);
    encode(bytes.data() + 8, size);
    return bytes;
}

} // namespace sim::judge::communication_protocol

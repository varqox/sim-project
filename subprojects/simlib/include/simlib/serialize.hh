#pragma once

#include <cstddef>
#include <cstring>
#include <exception>
#include <simlib/converts_safely_to.hh>
#include <simlib/macros/throw.hh>
#include <type_traits>
#include <utility>

namespace serialize {

enum class Phase {
    CountLen,
    Serialize,
};

template <Phase phase>
class Writer;

template <class T>
struct As {};
template <class T>
constexpr inline auto as = As<T>{};

template <class T>
struct CastedAs {};
template <class T>
constexpr inline auto casted_as = CastedAs<T>{};

template <Phase phase>
struct WriterBase {
    template <class T, std::enable_if_t<std::is_trivial_v<T>, int> = 0>
    void write_as_bytes(const T& value) {
        return static_cast<Writer<phase>*>(this)->write_bytes(&value, sizeof(T));
    }

    template <class T, class SerializedT, decltype(SerializedT{std::declval<T>()}, 0) = 0>
    void write(const T& value, As<SerializedT> /**/) {
        write_as_bytes(SerializedT{value});
    }

    template <class T, class SerializedT>
    void write(const T& value, CastedAs<SerializedT> /**/) {
        if (!converts_safely_to<SerializedT>(value)) {
            THROW("cannot safely convert value: ", value, " to ", typeid(SerializedT).name());
        }
        auto casted_value = static_cast<SerializedT>(value);
        write_as_bytes(casted_value);
    }

    template <
        size_t N,
        class SerializedFlagsT,
        std::enable_if_t<std::is_trivial_v<SerializedFlagsT>, int> = 0>
    void write_flags(
        std::pair<bool, SerializedFlagsT> (&&flags)[N], As<SerializedFlagsT> /**/
    ) {
        auto flags_to_write = SerializedFlagsT{};
        for (auto&& [flag, serialized_flag] : flags) {
            if (flag) {
                flags_to_write |= serialized_flag;
            }
        }
        write_as_bytes(flags_to_write);
    }
};

template <>
class Writer<Phase::CountLen> : public WriterBase<Phase::CountLen> {
    size_t size = 0;

public:
    Writer() noexcept = default;

    Writer(const Writer&) = delete;
    Writer(Writer&&) noexcept = default;
    Writer& operator=(const Writer&) = delete;
    Writer& operator=(Writer&&) noexcept = default;

    ~Writer() = default;

    void write_bytes(const void* /*ptr*/, size_t len) noexcept { size += len; }

    [[nodiscard]] size_t written_bytes_num() const noexcept { return size; }
};

template <>
class Writer<Phase::Serialize> : public WriterBase<Phase::Serialize> {
    std::byte* dest;
    size_t size;

public:
    Writer(std::byte* dest, size_t size) noexcept : dest{dest}, size{size} {}

    Writer(const Writer&) = delete;
    Writer(Writer&&) noexcept = default;
    Writer& operator=(const Writer&) = delete;
    Writer& operator=(Writer&&) noexcept = default;

    ~Writer() = default;

    void write_bytes(const void* ptr, size_t len) {
        if (len > size) {
            THROW("cannot write ", len, " bytes, have space only for ", size, " bytes");
        }
        std::memcpy(dest, ptr, len);
        dest += len;
        size -= len;
    }

    [[nodiscard]] size_t remaining_size() const noexcept { return size; }
};

} // namespace serialize

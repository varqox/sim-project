#pragma once

#include <cstddef>
#include <cstring>
#include <optional>
#include <simlib/converts_safely_to.hh>
#include <simlib/macros/throw.hh>
#include <type_traits>

namespace deserialize {

template <class T>
struct From {};
template <class T>
constexpr inline auto from = From<T>{};

template <class T>
struct CastedFrom {};
template <class T>
constexpr inline auto casted_from = CastedFrom<T>{};

class Reader {
    std::byte* data;
    size_t size;

public:
    Reader(std::byte* data, size_t size) noexcept : data{data}, size{size} {}

    Reader(const Reader&) = delete;
    Reader(Reader&&) = default;
    Reader& operator=(const Reader&) = delete;
    Reader& operator=(Reader&&) = default;

    ~Reader() = default;

    std::byte* extract_bytes(size_t len) {
        if (len > size) {
            THROW("cannot read ", len, " bytes, have only ", size, " bytes");
        }
        auto res = data;
        data += len;
        size -= len;
        return res;
    }

    void read_bytes(void* dest, size_t len) { std::memcpy(dest, extract_bytes(len), len); }

    template <class T, std::enable_if_t<std::is_trivial_v<T>, int> = 0>
    [[nodiscard]] T read_bytes_as() {
        T value;
        read_bytes(&value, sizeof(value));
        return value;
    }

    template <class T, class SerializedT, decltype(T{std::declval<SerializedT>()}, 0) = 0>
    [[nodiscard]] T read(From<SerializedT> /**/) {
        auto serialized_value = read_bytes_as<SerializedT>();
        return T{serialized_value};
    }

    template <class T, class SerializedT>
    [[nodiscard]] T read(CastedFrom<SerializedT> /**/) {
        auto serialized_value = read_bytes_as<SerializedT>();
        if (!converts_safely_to<T>(serialized_value)) {
            data -= sizeof(SerializedT);
            size += sizeof(SerializedT);
            THROW("Cannot safely convert value: ", serialized_value, " to ", typeid(T).name());
        }
        return static_cast<T>(serialized_value);
    }

    template <class T, class SerializedT>
    void read(T& x, From<SerializedT> /**/) {
        x = read<T>(from<SerializedT>);
    }

    template <class T, class SerializedT>
    void read(T& x, CastedFrom<SerializedT> /**/) {
        x = read<T>(casted_from<SerializedT>);
    }

    template <
        size_t N,
        class SerializedFlagsT,
        std::enable_if_t<std::is_trivial_v<SerializedFlagsT>, int> = 0>
    void read_flags(std::pair<bool&, SerializedFlagsT> (&&flags)[N], From<SerializedFlagsT> /**/) {
        auto read_flags = read_bytes_as<SerializedFlagsT>();
        for (auto&& [flag, serialized_flag] : flags) {
            if (read_flags & serialized_flag) {
                read_flags &= ~serialized_flag;
                flag = true;
            } else {
                flag = false;
            }
        }
        if (read_flags != 0) {
            THROW("found unexpected flags bits: (dec) ", read_flags);
        }
    }

    template <class T, class SerializedT>
    void read_optional_if(std::optional<T>& opt, From<SerializedT> /**/, bool condition) {
        if (condition) {
            opt = read<T>(from<SerializedT>);
        } else {
            opt = std::nullopt;
        }
    }

    template <class T, class SerializedT>
    void read_optional_if(std::optional<T>& opt, CastedFrom<SerializedT> /**/, bool condition) {
        if (condition) {
            opt = read<T>(casted_from<SerializedT>);
        } else {
            opt = std::nullopt;
        }
    }

    [[nodiscard]] std::optional<size_t> find_byte(std::byte b) {
        for (size_t pos = 0; pos < size; ++pos) {
            if (data[pos] == b) {
                return pos;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] size_t remaining_size() const noexcept { return size; }
};

} // namespace deserialize

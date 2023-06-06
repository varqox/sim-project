#pragma once

#include <cstddef>
#include <simlib/array_vec.hh>

template <size_t MAX_LEN>
class ArrayBuff : protected ArrayVec<std::byte, MAX_LEN> {
public:
    using value_type = typename ArrayVec<std::byte, MAX_LEN>::value_type;
    using size_type = typename ArrayVec<std::byte, MAX_LEN>::size_type;
    using reference = typename ArrayVec<std::byte, MAX_LEN>::reference;
    using const_reference = typename ArrayVec<std::byte, MAX_LEN>::const_reference;
    using pointer = typename ArrayVec<std::byte, MAX_LEN>::pointer;
    using const_pointer = typename ArrayVec<std::byte, MAX_LEN>::const_pointer;
    using ArrayVec<std::byte, MAX_LEN>::capacity;

    ArrayBuff() noexcept = default;
    ArrayBuff(const ArrayBuff&) noexcept = default;
    ArrayBuff(ArrayBuff&&) noexcept = default;
    ArrayBuff& operator=(const ArrayBuff&) noexcept = default;
    ArrayBuff& operator=(ArrayBuff&&) noexcept = default;
    ~ArrayBuff() = default;

    using ArrayVec<std::byte, MAX_LEN>::is_empty;
    using ArrayVec<std::byte, MAX_LEN>::size;
    using ArrayVec<std::byte, MAX_LEN>::max_size;
    using ArrayVec<std::byte, MAX_LEN>::data;
    using ArrayVec<std::byte, MAX_LEN>::operator[];
    using ArrayVec<std::byte, MAX_LEN>::resize;

    void append_bytes(const void* ptr, size_type len) noexcept {
        ArrayVec<std::byte, MAX_LEN>::append(static_cast<const_pointer>(ptr), len);
    }

    template <class T, std::enable_if_t<std::is_trivial_v<T>, int> = 0>
    void append_as_bytes(const T& val) noexcept {
        append_bytes(&val, sizeof(T));
    }
};

template <class... Ts, std::enable_if_t<(std::is_trivial_v<Ts> && ...), int> = 0>
ArrayBuff<(0 + ... + sizeof(Ts))> array_buff_from(const Ts&... vals) noexcept {
    ArrayBuff<(0 + ... + sizeof(Ts))> res;
    (res.append_as_bytes(vals), ...);
    return res;
}

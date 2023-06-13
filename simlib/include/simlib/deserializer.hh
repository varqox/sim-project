#pragma once

#include <cstddef>
#include <cstring>
#include <exception>
#include <type_traits>

class Deserializer {
private:
    const std::byte* data_;
    size_t size_;

public:
    constexpr Deserializer(const void* data, size_t size) noexcept
    : data_{static_cast<const std::byte*>(data)}
    , size_{size} {}

    constexpr Deserializer(const Deserializer&) noexcept = default;
    constexpr Deserializer(Deserializer&&) noexcept = default;
    constexpr Deserializer& operator=(const Deserializer&) noexcept = default;
    constexpr Deserializer& operator=(Deserializer&&) noexcept = default;

    ~Deserializer() = default;

    [[nodiscard]] constexpr bool is_empty() const noexcept { return size_ == 0; }

    [[nodiscard]] constexpr size_t size() const noexcept { return size_; }

    [[nodiscard]] constexpr const std::byte* data() const noexcept { return data_; }

    [[nodiscard]] constexpr const std::byte& operator[](size_t n) const noexcept {
        if (n >= size()) {
            std::terminate();
        }
        return data()[n];
    }

    Deserializer& deserialize_bytes(void* ptr, size_t len) noexcept {
        if (len > size()) {
            std::terminate();
        }
        std::memcpy(ptr, data(), len);
        data_ += len;
        size_ -= len;
        return *this;
    }

    template <class T, std::enable_if_t<std::is_trivial_v<T>, int> = 0>
    Deserializer& deserialize_bytes_as(T& val) noexcept {
        return deserialize_bytes(&val, sizeof(T));
    }

    Deserializer extract_bytes(size_t len) noexcept {
        if (len > size()) {
            std::terminate();
        }
        Deserializer res{data(), len};
        data_ += len;
        size_ -= len;
        return res;
    }
};

class MutDeserializer {
private:
    std::byte* data_;
    size_t size_;

public:
    constexpr MutDeserializer(void* data, size_t size) noexcept
    : data_{static_cast<std::byte*>(data)}
    , size_{size} {}

    constexpr MutDeserializer(const MutDeserializer&) noexcept = default;
    constexpr MutDeserializer(MutDeserializer&&) noexcept = default;
    constexpr MutDeserializer& operator=(const MutDeserializer&) noexcept = default;
    constexpr MutDeserializer& operator=(MutDeserializer&&) noexcept = default;

    ~MutDeserializer() = default;

    [[nodiscard]] constexpr bool is_empty() const noexcept { return size_ == 0; }

    [[nodiscard]] constexpr size_t size() const noexcept { return size_; }

    [[nodiscard]] constexpr std::byte* data() noexcept { return data_; }

    [[nodiscard]] constexpr const std::byte* data() const noexcept { return data_; }

    [[nodiscard]] constexpr std::byte& operator[](size_t n) noexcept {
        if (n >= size()) {
            std::terminate();
        }
        return data()[n];
    }

    [[nodiscard]] constexpr const std::byte& operator[](size_t n) const noexcept {
        if (n >= size()) {
            std::terminate();
        }
        return data()[n];
    }

    MutDeserializer& deserialize_bytes(void* ptr, size_t len) noexcept {
        if (len > size()) {
            std::terminate();
        }
        std::memcpy(ptr, data(), len);
        data_ += len;
        size_ -= len;
        return *this;
    }

    template <class T, std::enable_if_t<std::is_trivial_v<T>, int> = 0>
    MutDeserializer& deserialize_bytes_as(T& val) noexcept {
        return deserialize_bytes(&val, sizeof(T));
    }

    MutDeserializer extract_bytes(size_t len) noexcept {
        if (len > size()) {
            std::terminate();
        }
        MutDeserializer res{data(), len};
        data_ += len;
        size_ -= len;
        return res;
    }
};

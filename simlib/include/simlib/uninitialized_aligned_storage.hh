#pragma once

#include <cstddef>
#include <cstdint>

template <class AlignAsT, size_t N>
struct UninitializedAlignedStorage {
    alignas(AlignAsT) std::byte raw_data[sizeof(AlignAsT[N])];

    // Cannot be "= default", because then raw_data would be initialized but we want it to be
    // uninitialized
    // NOLINTNEXTLINE(modernize-use-equals-default)
    constexpr UninitializedAlignedStorage() noexcept {}

    constexpr UninitializedAlignedStorage(const UninitializedAlignedStorage&) noexcept = delete;

    constexpr UninitializedAlignedStorage(UninitializedAlignedStorage&&) noexcept = delete;

    constexpr UninitializedAlignedStorage&
    operator=(const UninitializedAlignedStorage&) noexcept = delete;

    constexpr UninitializedAlignedStorage&
    operator=(UninitializedAlignedStorage&&) noexcept = delete;

    ~UninitializedAlignedStorage() = default;

    [[nodiscard]] std::byte* data() noexcept { return raw_data; }

    [[nodiscard]] volatile std::byte* data() volatile noexcept { return raw_data; }

    [[nodiscard]] const std::byte* data() const noexcept { return raw_data; }

    [[nodiscard]] volatile const std::byte* data() volatile const noexcept { return raw_data; }
};

template <class AlignAsT>
struct UninitializedAlignedStorage<AlignAsT, 0> {
    constexpr UninitializedAlignedStorage() noexcept = default;

    constexpr UninitializedAlignedStorage(const UninitializedAlignedStorage&) noexcept = delete;

    constexpr UninitializedAlignedStorage(UninitializedAlignedStorage&&) noexcept = delete;

    constexpr UninitializedAlignedStorage&
    operator=(const UninitializedAlignedStorage&) noexcept = delete;

    constexpr UninitializedAlignedStorage&
    operator=(UninitializedAlignedStorage&&) noexcept = delete;

    ~UninitializedAlignedStorage() = default;

    [[nodiscard]] std::byte* data() noexcept { return nullptr; }

    [[nodiscard]] volatile std::byte* data() volatile noexcept { return nullptr; }

    [[nodiscard]] const std::byte* data() const noexcept { return nullptr; }

    [[nodiscard]] volatile const std::byte* data() volatile const noexcept { return nullptr; }
};

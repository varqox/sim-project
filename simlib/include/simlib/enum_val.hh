#pragma once

#include "enum_with_string_conversions.hh"

#include <optional>
#include <simlib/always_false.hh>
#include <simlib/string_transform.hh>
#include <simlib/string_view.hh>
#include <type_traits>

template <class Enum, class = void>
class EnumVal {
    static_assert(always_false<Enum>, "EnumVal is designed only for enum-like types");
};

template <class Enum>
class EnumVal<Enum, std::enable_if_t<std::is_enum_v<Enum>>> {
public:
    using EnumType = Enum;
    using ValType = std::underlying_type_t<Enum>;

private:
    ValType val_;

public:
    EnumVal() = default;
    EnumVal(const EnumVal&) = default;
    EnumVal(EnumVal&&) noexcept = default;
    EnumVal& operator=(const EnumVal&) = default;
    EnumVal& operator=(EnumVal&&) noexcept = default;

    ~EnumVal() = default;

    constexpr explicit EnumVal(ValType val) : val_(val) {}

    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr EnumVal(Enum val) : val_(static_cast<ValType>(val)) {}

    constexpr EnumVal& operator=(Enum val) {
        val_ = static_cast<ValType>(val);
        return *this;
    }

    [[nodiscard]] constexpr Enum to_enum() const noexcept { return Enum{val_}; }

    [[nodiscard]] constexpr ValType to_int() const noexcept { return val_; }

    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr operator Enum() const noexcept { return to_enum(); }

    constexpr explicit operator ValType&() & noexcept { return val_; }

    constexpr explicit operator const ValType&() const& noexcept { return val_; }
};

template <class Enum>
class EnumVal<Enum, std::enable_if_t<is_enum_with_string_conversions<Enum>>> {
public:
    using EnumType = Enum;
    using ValType = typename Enum::UnderlyingType;

private:
    ValType val_;

public:
    EnumVal() = default;
    EnumVal(const EnumVal&) = default;
    EnumVal(EnumVal&&) noexcept = default;
    EnumVal& operator=(const EnumVal&) = default;
    EnumVal& operator=(EnumVal&&) noexcept = default;

    ~EnumVal() = default;

    constexpr explicit EnumVal(ValType val) : val_(val) {}

    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr EnumVal(Enum val) : val_(static_cast<ValType>(val)) {}

    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr EnumVal(typename Enum::Enum val) : val_(static_cast<ValType>(val)) {}

    constexpr EnumVal& operator=(Enum val) {
        val_ = static_cast<ValType>(val);
        return *this;
    }

    constexpr EnumVal& operator=(typename Enum::Enum val) {
        val_ = static_cast<ValType>(val);
        return *this;
    }

    [[nodiscard]] constexpr Enum to_enum() const noexcept { return Enum{val_}; }

    [[nodiscard]] constexpr ValType to_int() const noexcept { return val_; }

    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr operator Enum() const noexcept { return to_enum(); }

    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr operator typename Enum::Enum() const noexcept { return to_enum(); }

    constexpr explicit operator ValType&() & noexcept { return val_; }

    constexpr explicit operator const ValType&() const& noexcept { return val_; }

    static constexpr std::optional<EnumVal> from_str(StringView str) noexcept {
        return Enum::from_str(str);
    }
};

template <class Enum>
EnumVal(Enum) -> EnumVal<Enum>;

template <class...>
constexpr inline bool is_enum_val = false;
template <class T>
constexpr inline bool is_enum_val<EnumVal<T>> = true;

template <class...>
constexpr inline bool is_enum_val_with_string_conversions = false;
template <class T>
constexpr inline bool is_enum_val_with_string_conversions<EnumVal<T>> =
    is_enum_with_string_conversions<T>;

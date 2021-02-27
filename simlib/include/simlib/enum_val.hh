#pragma once

#include <type_traits>

template <class Enum>
class EnumVal {
public:
    static_assert(std::is_enum<Enum>::value, "EnumVal is designed only for enums");
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

    constexpr explicit EnumVal(ValType val)
    : val_(val) {}

    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr EnumVal(Enum val)
    : val_(static_cast<ValType>(val)) {}

    constexpr EnumVal& operator=(Enum val) {
        val_ = static_cast<ValType>(val);
        return *this;
    }

    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr operator Enum() const noexcept { return Enum(val_); }

    [[nodiscard]] constexpr ValType int_val() const noexcept { return val_; }

    constexpr explicit operator ValType&() & noexcept { return val_; }

    constexpr explicit operator const ValType&() const& noexcept { return val_; }
};

template <class...>
constexpr inline bool is_enum_val = false;
template <class T>
constexpr inline bool is_enum_val<EnumVal<T>> = true;

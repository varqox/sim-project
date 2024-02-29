#pragma once

#include <simlib/is_printable.hh>
#include <type_traits>
#include <utility>
#include <variant>

template <class T>
struct Ok {
    T val;

    constexpr explicit Ok(T val) noexcept : val{std::move(val)} {}

    Ok(const Ok&) = default;
    Ok(Ok&&) noexcept = default;
    Ok& operator=(const Ok&) = default;
    Ok& operator=(Ok&&) noexcept = default;
    ~Ok() = default;

    template <class U, std::enable_if_t<std::is_constructible_v<T, U&&>, int> = 0>
    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr Ok(Ok<U>&& other) noexcept : val{std::move(other.val)} {}
};

template <>
struct Ok<void> {
    constexpr Ok() noexcept = default;

    Ok(const Ok&) = default;
    Ok(Ok&&) noexcept = default;
    Ok& operator=(const Ok&) = default;
    Ok& operator=(Ok&&) noexcept = default;
    ~Ok() = default;
};

Ok() -> Ok<void>;

inline std::ostream& operator<<(std::ostream& os, const Ok<void>& /*ok*/) { return os << "Ok{}"; }

template <class T, std::enable_if_t<is_printable<T>, int> = 0>
std::ostream& operator<<(std::ostream& os, const Ok<T>& ok) {
    return os << "Ok{" << ok.val << "}";
}

template <class A, class B>
constexpr bool operator==(const Ok<A>& a, const Ok<B>& b) {
    if constexpr (std::is_same_v<A, void> && std::is_same_v<B, void>) {
        return true;
    } else if constexpr (std::is_same_v<A, void> || std::is_same_v<B, void>) {
        return false;
    } else {
        return a.val == b.val;
    }
}

template <class A, class B>
constexpr bool operator!=(const Ok<A>& a, const Ok<B>& b) {
    return !(a == b);
}

template <class T>
struct Err {
    T err;

    constexpr explicit Err(T err) noexcept : err{std::move(err)} {}

    Err(const Err&) = default;
    Err(Err&&) noexcept = default;
    Err& operator=(const Err&) = default;
    Err& operator=(Err&&) noexcept = default;
    ~Err() = default;

    template <class U, std::enable_if_t<std::is_constructible_v<T, U&&>, int> = 0>
    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr Err(Err<U>&& other) noexcept : err{std::move(other.err)} {}
};

template <>
struct Err<void> {
    constexpr Err() noexcept = default;

    Err(const Err&) = default;
    Err(Err&&) noexcept = default;
    Err& operator=(const Err&) = default;
    Err& operator=(Err&&) noexcept = default;
    ~Err() = default;
};

Err() -> Err<void>;

inline std::ostream& operator<<(std::ostream& os, const Err<void>& /*err*/) {
    return os << "Err{}";
}

template <class T, std::enable_if_t<is_printable<T>, int> = 0>
std::ostream& operator<<(std::ostream& os, const Err<T>& err) {
    return os << "Err{" << err.err << "}";
}

template <class A, class B>
constexpr bool operator==(const Err<A>& a, const Err<B>& b) {
    if constexpr (std::is_same_v<A, void> && std::is_same_v<B, void>) {
        return true;
    } else if constexpr (std::is_same_v<A, void> || std::is_same_v<B, void>) {
        return false;
    } else {
        return a.err == b.err;
    }
}

template <class A, class B>
constexpr bool operator!=(const Err<A>& a, const Err<B>& b) {
    return !(a == b);
}

template <class T, class E>
struct Result : std::variant<Ok<T>, Err<E>> {
    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr Result(Ok<T> ok) : std::variant<Ok<T>, Err<E>>{std::move(ok)} {}

    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr Result(Err<E> err) : std::variant<Ok<T>, Err<E>>{std::move(err)} {}

    [[nodiscard]] constexpr bool is_ok() const noexcept {
        return std::holds_alternative<Ok<T>>(*this);
    }

    [[nodiscard]] constexpr bool is_err() const noexcept {
        return std::holds_alternative<Err<E>>(*this);
    }

    constexpr T unwrap() && noexcept {
        if constexpr (std::is_same_v<T, void>) {
            return;
        } else {
            return std::get<Ok<T>>(std::move(*this)).val;
        }
    }

    constexpr E unwrap_err() && noexcept {
        if constexpr (std::is_same_v<E, void>) {
            return;
        } else {
            return std::get<Err<E>>(std::move(*this)).err;
        }
    }
};

template <
    class TA,
    class EA,
    class TB,
    class EB,
    std::enable_if_t<!std::is_same_v<TA, TB> or !std::is_same_v<EA, EB>, int> = 0>
constexpr bool operator==(const Result<TA, EA>& a, const Result<TB, EB>& b) {
    if (a.index() != b.index()) {
        return false;
    }
    if (a.index() == 0) {
        return std::get<0>(a) == std::get<0>(b);
    }
    return std::get<1>(a) == std::get<1>(b);
}

template <
    class TA,
    class EA,
    class TB,
    class EB,
    std::enable_if_t<!std::is_same_v<TA, TB> or !std::is_same_v<EA, EB>, int> = 0>
constexpr bool operator!=(const Result<TA, EA>& a, const Result<TB, EB>& b) {
    return !(a == b);
}

template <class T, class E, class X>
constexpr bool operator==(const Result<T, E>& r, const Ok<X>& x) {
    return std::visit(
        [&](const auto& a) {
            if constexpr (std::is_same_v<decltype(a), const Ok<T>&>) {
                return a == x;
            } else {
                return false;
            }
        },
        r
    );
}

template <class T, class E, class X>
constexpr bool operator==(const Result<T, E>& r, const Err<X>& x) {
    return std::visit(
        [&](const auto& a) {
            if constexpr (std::is_same_v<decltype(a), const Err<E>&>) {
                return a == x;
            } else {
                return false;
            }
        },
        r
    );
}

template <class T, class E, class X>
constexpr bool operator!=(const Result<T, E>& r, const Ok<X>& x) {
    return !(r == x);
}

template <class T, class E, class X>
constexpr bool operator!=(const Result<T, E>& r, const Err<X>& x) {
    return !(r == x);
}

template <class X, class T, class E>
constexpr bool operator==(const Ok<X>& x, const Result<T, E>& r) {
    return r == x;
}

template <class X, class T, class E>
constexpr bool operator==(const Err<X>& x, const Result<T, E>& r) {
    return r == x;
}

template <class X, class T, class E>
constexpr bool operator!=(const Ok<X>& x, const Result<T, E>& r) {
    return r != x;
}

template <class X, class T, class E>
constexpr bool operator!=(const Err<X>& x, const Result<T, E>& r) {
    return r != x;
}

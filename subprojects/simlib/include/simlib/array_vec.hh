#pragma once

#include <cstddef>
#include <exception>
#include <simlib/macros/likely.hh>
#include <simlib/uninitialized_aligned_storage.hh>
#include <type_traits>
#include <utility>

template <class T, size_t MAX_SIZE>
class ArrayVec {
public:
    using value_type = T;
    using size_type = size_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = pointer;
    using const_iterator = const_pointer;
    static constexpr inline size_type capacity = MAX_SIZE;

private:
    UninitializedAlignedStorage<value_type, MAX_SIZE> raw_data;
    size_type len = 0;

    constexpr void destruct() {
        auto i = len;
        while (i > 0) {
            --i;
            begin()[i].~value_type();
        }
    }

public:
    ~ArrayVec() { destruct(); }

    constexpr ArrayVec() noexcept = default;

    template <
        class Arg,
        std::enable_if_t<
            !std::is_same_v<std::remove_cv_t<std::remove_reference_t<Arg>>, ArrayVec> &&
                std::is_constructible_v<value_type, Arg&&>,
            int> = 0>
    constexpr explicit ArrayVec(Arg&& arg) {
        emplace(std::forward<Arg>(arg));
    }

    template <class Arg1, class Arg2, class... Args>
    ArrayVec(Arg1&& arg1, Arg2&& arg2, Args&&... args) {
        try {
            emplace(std::forward<Arg1>(arg1));
            emplace(std::forward<Arg2>(arg2));
            (emplace(std::forward<Args>(args)), ...);
        } catch (...) {
            destruct();
            throw;
        }
    }

    template <class... Args>
    static constexpr ArrayVec from(Args&&... args) {
        static_assert(sizeof...(args) <= MAX_SIZE);
        ArrayVec av;
        (av.emplace(std::forward<Args>(args)), ...);
        return av;
    }

    template <class Iter>
    static constexpr ArrayVec from_range(Iter beg, Iter end) {
        ArrayVec av;
        while (beg != end) {
            av.emplace(*beg);
            ++beg;
        }
        return av;
    }

    ArrayVec(const ArrayVec& other) {
        try {
            for (const auto& val : other) {
                emplace(val);
            }
        } catch (...) {
            destruct();
            throw;
        }
    }

    // NOLINTNEXTLINE(performance-noexcept-move-constructor)
    ArrayVec(ArrayVec&& other) {
        try {
            for (auto& val : other) {
                emplace(std::move(val));
            }
        } catch (...) {
            destruct();
            throw;
        }
    }

    template <
        class OTHER_T,
        size_type OTHER_MAX_SIZE,
        std::enable_if_t<!std::is_same_v<T, OTHER_T> || MAX_SIZE != OTHER_MAX_SIZE, int> = 0>
    explicit ArrayVec(const ArrayVec<OTHER_T, OTHER_MAX_SIZE>& other) {
        if (UNLIKELY(other.size() > MAX_SIZE)) {
            std::terminate();
        }
        try {
            for (const auto& val : other) {
                emplace(val);
            }
        } catch (...) {
            destruct();
            throw;
        }
    }

    template <
        class OTHER_T,
        size_type OTHER_MAX_SIZE,
        std::enable_if_t<!std::is_same_v<T, OTHER_T> || MAX_SIZE != OTHER_MAX_SIZE, int> = 0>
    explicit ArrayVec(ArrayVec<OTHER_T, OTHER_MAX_SIZE>&& other) {
        if (UNLIKELY(other.size() > MAX_SIZE)) {
            std::terminate();
        }
        try {
            for (auto& val : other) {
                emplace(std::move(val));
            }
        } catch (...) {
            destruct();
            throw;
        }
    }

    template <class OTHER_T, size_type OTHER_MAX_SIZE>
    constexpr ArrayVec& operator=(const ArrayVec<OTHER_T, OTHER_MAX_SIZE>& other) {
        if (UNLIKELY(this == static_cast<const void*>(&other))) {
            return *this;
        }
        if (UNLIKELY(other.size() > MAX_SIZE)) {
            std::terminate();
        }
        shrink_to_if_larger(other.size());
        for (size_type i = 0; i < size(); ++i) {
            (*this)[i] = other[i];
        }
        while (size() < other.size()) {
            emplace(other[size()]);
        }
        return *this;
    }

    template <class OTHER_T, size_type OTHER_MAX_SIZE>
    constexpr ArrayVec& operator=(ArrayVec<OTHER_T, OTHER_MAX_SIZE>&& other) {
        if (UNLIKELY(this == static_cast<const void*>(&other))) {
            return *this;
        }
        if (UNLIKELY(other.size() > MAX_SIZE)) {
            std::terminate();
        }
        shrink_to_if_larger(other.size());
        for (size_type i = 0; i < size(); ++i) {
            (*this)[i] = std::move(other[i]);
        }
        while (size() < other.size()) {
            emplace(std::move(other[size()]));
        }
        return *this;
    }

    constexpr ArrayVec& operator=(const ArrayVec& other) {
        if (UNLIKELY(this == &other)) {
            return *this;
        }
        operator= <T, MAX_SIZE>(other);
        return *this;
    }

    // NOLINTNEXTLINE(performance-noexcept-move-constructor)
    constexpr ArrayVec& operator=(ArrayVec&& other) {
        if (UNLIKELY(this == &other)) {
            return *this;
        }
        operator= <T, MAX_SIZE>(std::move(other));
        return *this;
    }

    [[nodiscard]] constexpr bool is_empty() const noexcept { return len == 0; }

    [[nodiscard]] constexpr size_type size() const noexcept { return len; }

    [[nodiscard]] constexpr size_type max_size() const noexcept { return MAX_SIZE; }

    [[nodiscard]] constexpr pointer data() noexcept {
        return reinterpret_cast<pointer>(raw_data.data());
    }

    [[nodiscard]] constexpr const_pointer data() const noexcept {
        return reinterpret_cast<const_pointer>(raw_data.data());
    }

    [[nodiscard]] constexpr iterator begin() noexcept { return data(); }

    [[nodiscard]] constexpr const_iterator begin() const noexcept { return data(); }

    [[nodiscard]] constexpr iterator end() noexcept { return begin() + size(); }

    [[nodiscard]] constexpr const_iterator end() const noexcept { return begin() + size(); }

    [[nodiscard]] constexpr reference front() noexcept { return *begin(); }

    [[nodiscard]] constexpr const_reference front() const noexcept { return *begin(); }

    [[nodiscard]] constexpr reference back() noexcept { return *(end() - 1); }

    [[nodiscard]] constexpr const_reference back() const noexcept { return *(end() - 1); }

    [[nodiscard]] constexpr reference operator[](size_type n) noexcept {
        if (UNLIKELY(n >= size())) {
            std::terminate();
        }
        return data()[n];
    }

    [[nodiscard]] constexpr const_reference operator[](size_type n) const noexcept {
        if (UNLIKELY(n >= size())) {
            std::terminate();
        }
        return data()[n];
    }

    template <class... Args>
    [[nodiscard]] constexpr bool try_emplace(Args&&... args) {
        if (len == MAX_SIZE) {
            return false;
        }
        new (data() + len) value_type{std::forward<Args>(args)...};
        ++len;
        return true;
    }

    template <class... Args>
    constexpr void emplace(Args&&... args) {
        if (UNLIKELY(len == MAX_SIZE)) {
            std::terminate();
        }
        new (data() + len) value_type{std::forward<Args>(args)...};
        ++len;
    }

    [[nodiscard]] constexpr bool try_push(const value_type& val) { return try_emplace(val); }

    [[nodiscard]] constexpr bool try_push(value_type&& val) { return try_emplace(std::move(val)); }

    constexpr void push(const value_type& val) { return emplace(val); }

    constexpr void push(value_type&& val) { return emplace(std::move(val)); }

    template <class Iter>
    constexpr void append(Iter beg, size_type n) {
        if (UNLIKELY(n > MAX_SIZE - len)) {
            std::terminate();
        }
        while (n > 0) {
            emplace(*beg);
            ++beg;
            --n;
        }
    }

    constexpr void pop() {
        if (UNLIKELY(is_empty())) {
            std::terminate();
        }
        --len;
        end()->~value_type();
    }

    [[nodiscard]] constexpr value_type pop_value() {
        if (UNLIKELY(is_empty())) {
            std::terminate();
        }
        value_type val = std::move(back());
        --len;
        end()->~value_type();
        return val;
    }

    constexpr void clear() {
        while (len > 0) {
            --len;
            end()->~value_type();
        }
    }

private:
    constexpr void shrink_to_if_larger(size_type count) {
        while (len > count) {
            --len;
            end()->~value_type();
        }
    }

public:
    constexpr void resize(size_type count) {
        if (UNLIKELY(count > MAX_SIZE)) {
            std::terminate();
        }
        shrink_to_if_larger(count);
        while (len < count) {
            new (data() + len) value_type{};
            ++len;
        }
    }

    template <class Arg>
    constexpr void resize(size_type count, Arg&& value) {
        if (UNLIKELY(count > MAX_SIZE)) {
            std::terminate();
        }
        shrink_to_if_larger(count);
        while (len < count) {
            new (data() + len) value_type{value};
            ++len;
        }
    }
};

template <class Arg>
ArrayVec(Arg&&) -> ArrayVec<Arg, 1>;

template <class Arg1, class... Args, std::enable_if_t<(std::is_same_v<Arg1, Args> && ...), int> = 0>
ArrayVec(Arg1&&, Args&&...) -> ArrayVec<Arg1, 1 + sizeof...(Args)>;

template <class A_T, size_t A_MAX_SIZE, class B_T, size_t B_MAX_SIZE>
[[nodiscard]] constexpr bool
operator==(const ArrayVec<A_T, A_MAX_SIZE>& a, const ArrayVec<B_T, B_MAX_SIZE>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (decltype(a.size()) i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

template <class A_T, size_t A_MAX_SIZE, class B_T, size_t B_MAX_SIZE>
[[nodiscard]] constexpr bool
operator!=(const ArrayVec<A_T, A_MAX_SIZE>& a, const ArrayVec<B_T, B_MAX_SIZE>& b) {
    return !(a == b);
}

template <class A_T, size_t A_MAX_SIZE, class B_T, size_t B_MAX_SIZE>
[[nodiscard]] constexpr bool
operator<(const ArrayVec<A_T, A_MAX_SIZE>& a, const ArrayVec<B_T, B_MAX_SIZE>& b) {
    decltype(a.size()) i = 0;
    for (; i < a.size() && i < b.size(); ++i) {
        if (a[i] == b[i]) {
            continue;
        }
        if (a[i] < b[i]) {
            return true;
        }
        return false;
    }
    return (i == a.size()) && (i != b.size());
}

template <class A_T, size_t A_MAX_SIZE, class B_T, size_t B_MAX_SIZE>
[[nodiscard]] constexpr bool
operator>(const ArrayVec<A_T, A_MAX_SIZE>& a, const ArrayVec<B_T, B_MAX_SIZE>& b) {
    return b < a;
}

template <class A_T, size_t A_MAX_SIZE, class B_T, size_t B_MAX_SIZE>
[[nodiscard]] constexpr bool
operator<=(const ArrayVec<A_T, A_MAX_SIZE>& a, const ArrayVec<B_T, B_MAX_SIZE>& b) {
    return !(a > b);
}

template <class A_T, size_t A_MAX_SIZE, class B_T, size_t B_MAX_SIZE>
[[nodiscard]] constexpr bool
operator>=(const ArrayVec<A_T, A_MAX_SIZE>& a, const ArrayVec<B_T, B_MAX_SIZE>& b) {
    return !(a < b);
}

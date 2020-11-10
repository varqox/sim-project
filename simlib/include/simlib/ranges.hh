#pragma once

#include <iterator>
#include <tuple>

namespace detail {

template <class T>
struct reverse_view_impl {
    int prevent_copy_elision_; // Used to allow reverse_view(reverse_view(...))
    T range;

    constexpr auto begin() { return std::rbegin(range); }
    constexpr auto end() { return std::rend(range); }
    constexpr auto rbegin() { return std::begin(range); }
    constexpr auto rend() { return std::end(range); }
};

template <class T>
reverse_view_impl(int, T&&) -> reverse_view_impl<T&&>;

} // namespace detail

// This way it will work without with temporaries calling any constructor: e.g.
#define reverse_view(...) \
    detail::reverse_view_impl { 0, __VA_ARGS__ }

namespace detail {

template <class Iter, class ContainerT>
struct enumerate_view_iter {
    size_t idx;
    Iter iter;

    friend bool operator!=(const enumerate_view_iter& a, const enumerate_view_iter& b) {
        return (a.iter != b.iter);
    }

    void operator++() {
        ++idx;
        ++iter;
    }

    using IterTraits = std::iterator_traits<Iter>;

    std::conditional_t<
        std::is_const_v<std::remove_reference_t<typename IterTraits::reference>>,
        const enumerate_view_iter&, enumerate_view_iter&>
    operator*() {
        return *this;
    }

    template <size_t I, std::enable_if_t<I == 0, int> = 0>
    [[nodiscard]] size_t get() const noexcept {
        return idx;
    }

    template <size_t I, std::enable_if_t<I == 1, int> = 0>
    typename IterTraits::reference get() & {
        return *iter;
    }

    template <size_t I, std::enable_if_t<I == 1, int> = 0>
    // NOLINTNEXTLINE(readability-const-return-type)
    [[nodiscard]] const typename IterTraits::reference get() const& {
        return *iter;
    }

    template <size_t I, std::enable_if_t<I == 1, int> = 0>
    typename IterTraits::value_type get() && {
        return *iter;
    }
};

} // namespace detail

namespace std {

template <class Iter, class ContainerT>
struct tuple_size<detail::enumerate_view_iter<Iter, ContainerT>>
: std::integral_constant<size_t, 2> {};

template <class Iter, class ContainerT>
struct tuple_element<0, detail::enumerate_view_iter<Iter, ContainerT>> {
    using type = size_t;
};

template <class Iter, class ContainerT>
struct tuple_element<0, const detail::enumerate_view_iter<Iter, ContainerT>> {
    using type = size_t;
};

template <class Iter, class ContainerT>
struct tuple_element<1, detail::enumerate_view_iter<Iter, ContainerT>> {
    using type = typename std::iterator_traits<Iter>::value_type;
};

} // namespace std

namespace detail {

template <class T>
struct enumerate_view_struct {
    int prevent_copy_elision_; // Used to allow reverse_view(reverse_view(...))
    T range;

    constexpr auto begin() {
        return enumerate_view_iter<decltype(std::begin(range)), T>{0, std::begin(range)};
    }
    constexpr auto end() {
        return enumerate_view_iter<decltype(std::begin(range)), T>{0, std::end(range)};
    }
};

template <class T>
enumerate_view_struct(int, T&&) -> enumerate_view_struct<T&&>;

} // namespace detail

/// Intended for use in range-based for loop along with structured binding e.g.
/// ```cpp
/// std::vector<int> v {1, 2, 3};
/// for (auto& [i, x] : enumerate_view(v)) {
///     std::cout << i << ": " << x << std::endl;
///     ++x; // modifies value in v because it is captured by reference
/// }
/// for (auto [i, x] : enumerate_view(v)) {
///     std::cout << i << ": " << x << std::endl;
/// }
/// ```
#define enumerate_view(...) \
    detail::enumerate_view_struct { 0, __VA_ARGS__ }

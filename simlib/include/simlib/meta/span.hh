#pragma once

#include <cstddef>
#include <utility>

namespace meta {

namespace detail {

template <size_t beg, size_t count, size_t... indexes>
struct create_span_impl {
    using type = typename create_span_impl<beg + 1, count - 1, indexes..., beg>::type;
};

template <size_t beg, size_t... indexes>
struct create_span_impl<beg, 0, indexes...> {
    using type = std::index_sequence<indexes...>;
};

} // namespace detail

// Equal to std::index_sequence<beg, beg + 1, ..., end - 1>
template <size_t beg, size_t end>
using span = typename detail::create_span_impl<beg, end - beg>::type;

} // namespace meta

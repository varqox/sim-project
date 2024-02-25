#include <iterator>
#include <utility>

// Merges
template <
    class C,
    class R,
    decltype(std::inserter(std::declval<C&>(), std::declval<C&>().end()), std::begin(std::declval<const R&>()), std::end(std::declval<const R&>()), 0) =
        0>
[[nodiscard]] constexpr C merge(C container, const R& range) {
    auto inserter = std::inserter(container, container.end());
    for (auto&& e : range) {
        *inserter = e;
        ++inserter;
    }
    return container;
}

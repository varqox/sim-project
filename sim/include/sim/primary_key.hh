#pragma once

#include <tuple>
#include <type_traits>
#include <utility>

namespace sim {

template <class Class, class... T>
class PrimaryKey {
    std::tuple<T Class::*...> member_ptrs;

public:
    using Type = std::conditional_t<sizeof...(T) == 1, std::tuple_element_t<0, std::tuple<T...>>,
            std::tuple<T...>>;

    constexpr explicit PrimaryKey(T Class::*... member_ptrs)
    : member_ptrs{member_ptrs...} {}

    [[nodiscard]] Type get(const Class& obj) const {
        return std::apply(do_get, std::tuple_cat(std::tie(obj), member_ptrs));
    }

    void set(Class& obj, Type val) const {
        std::apply(do_set, std::tuple_cat(std::tie(obj), member_ptrs)) = std::tuple{std::move(val)};
    }

private:
    static Type do_get(const Class& obj, T Class::*... ptrs) { return {obj.*ptrs...}; }

    static std::tuple<T&...> do_set(Class& obj, T Class::*... ptrs) {
        return std::tie(obj.*ptrs...);
    }
};

} // namespace sim

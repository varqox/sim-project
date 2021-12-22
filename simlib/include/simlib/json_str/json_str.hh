#pragma once

#include "simlib/concat_tostr.hh"
#include "simlib/enum_val.hh"
#include "simlib/enum_with_string_conversions.hh"
#include "simlib/mysql/mysql.hh"
#include "simlib/string_transform.hh"
#include "simlib/string_view.hh"

#include <cstddef>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

namespace json_str {

namespace detail {

template <class...>
static constexpr inline bool is_std_optional = false;
template <class T>
static constexpr inline bool is_std_optional<std::optional<T>> = true;

} // namespace detail

class Builder {
protected:
    std::string& str;

    explicit Builder(std::string& str)
    : str{str} {}

    template <class... Arg>
    void append_raw_value(Arg&&... arg) {
        back_insert(str, std::forward<Arg>(arg)..., ',');
    }

    template <class T>
    void append_value(T&& val) {
        using DT = std::decay_t<T>;
        if constexpr (std::is_same_v<DT, bool>) {
            append_raw_value(val ? "true" : "false");
        } else if constexpr (std::is_same_v<DT, std::nullptr_t> or
                std::is_same_v<DT, std::nullopt_t>) {
            append_raw_value("null");
        } else if constexpr (std::is_integral_v<DT>) {
            append_raw_value(val);
        } else if constexpr (detail::is_std_optional<DT> or mysql::is_optional<DT>) {
            if (val) {
                append_value(*val);
            } else {
                append_raw_value("null");
            }
        } else if constexpr (is_enum_with_string_conversions<DT>) {
            append_raw_value(val.to_quoted_str());
        } else if constexpr (is_enum_val_with_string_conversions<DT>) {
            append_raw_value(val.to_enum().to_quoted_str());
        } else {
            append_stringified_json(str, std::forward<T>(val));
            back_insert(str, ',');
        }
    }

    template <class Func>
    void append_arr(Func&& func);

    template <class Func>
    void append_obj(Func&& func);
};

class ArrayBuilder : Builder {
    friend class Builder;
    friend class Array;

    explicit ArrayBuilder(std::string& str)
    : Builder{str} {
        str += '[';
    }

    void end() {
        if (str.back() == ',') {
            str.back() = ']';
        } else {
            back_insert(str, ']');
        }
    }

public:
    template <class... Arg>
    void val_raw(Arg&&... arg) {
        append_raw_value(std::forward<Arg>(arg)...);
    }

    template <class T>
    void val(T&& val) {
        append_value(std::forward<T>(val));
    }

    template <class Func>
    void val_arr(Func&& func) {
        append_arr(std::forward<Func>(func));
    }

    template <class Func>
    void val_obj(Func&& func) {
        append_obj(std::forward<Func>(func));
    }
};

class ObjectBuilder : Builder {
    friend class Builder;
    friend class Object;

    explicit ObjectBuilder(std::string& str)
    : Builder{str} {
        str += '{';
    }

    void end() {
        if (str.back() == ',') {
            str.back() = '}';
        } else {
            back_insert(str, '}');
        }
    }

    void append_prop_name(StringView name) { back_insert(str, '"', name, "\":"); }

public:
    template <class... Arg>
    void prop_raw(StringView name, Arg&&... arg) {
        append_prop_name(name);
        append_raw_value(std::forward<Arg>(arg)...);
    }

    template <class T>
    void prop(StringView name, T&& val) {
        append_prop_name(name);
        append_value(std::forward<T>(val));
    }

    template <class Func>
    void prop_arr(StringView name, Func&& func) {
        append_prop_name(name);
        append_arr(std::forward<Func>(func));
    }

    template <class Func>
    void prop_obj(StringView name, Func&& func) {
        append_prop_name(name);
        append_obj(std::forward<Func>(func));
    }
};

template <class Func>
void Builder::append_arr(Func&& func) {
    static_assert(std::is_invocable_v<Func&&, ArrayBuilder&>);
    auto arr = ArrayBuilder{str};
    std::forward<Func>(func)(arr);
    arr.end();
    back_insert(str, ',');
}

template <class Func>
void Builder::append_obj(Func&& func) {
    static_assert(std::is_invocable_v<Func&&, ObjectBuilder&>);
    auto obj = ObjectBuilder{str};
    std::forward<Func>(func)(obj);
    obj.end();
    back_insert(str, ',');
}

namespace detail {
struct StringWrapper {
    std::string str;
};
} // namespace detail

class Object
: detail::StringWrapper
, public ObjectBuilder {
public:
    Object()
    : ObjectBuilder{StringWrapper::str} {}

    std::string into_str() && {
        end();
        return std::move(StringWrapper::str);
    }
};

class Array
: detail::StringWrapper
, public ArrayBuilder {
public:
    Array()
    : ArrayBuilder{StringWrapper::str} {}

    std::string into_str() && {
        end();
        return std::move(StringWrapper::str);
    }
};

} // namespace json_str

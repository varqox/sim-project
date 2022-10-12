#pragma once

#include "simlib/concat_tostr.hh"
#include "simlib/string_view.hh"

#include <tuple>
#include <type_traits>
#include <utility>

namespace sql {

template <class... Params>
class Condition {
    std::string sql_str;
    std::tuple<Params...> params;

public:
    explicit Condition(std::string condition_sql_str, Params&&... params);

    Condition(const Condition&) = default;
    Condition(Condition&&) noexcept = default;
    Condition& operator=(const Condition&) = delete;
    Condition& operator=(Condition&&) noexcept = default;
    ~Condition() = default;

private:
    Condition(std::string condition_sql_str, std::tuple<Params...> params);

    template <class... ParamsA, class... ParamsB>
    friend Condition<ParamsA..., ParamsB...> operator and(
            Condition<ParamsA...> a, Condition<ParamsB...> b);

    template <class... ParamsA, class... ParamsB>
    friend Condition<ParamsA..., ParamsB...> operator or(
            Condition<ParamsA...> a, Condition<ParamsB...> b);

    template <class...>
    friend class SelectFrom;
    template <class...>
    friend class SelectJoin;
    template <class...>
    friend class SelectJoinOn;
};

template <class... Params>
Condition(std::string, Params&&...) -> Condition<Params...>;

inline const auto TRUE = Condition{"TRUE"};
inline const auto FALSE = Condition{"FALSE"};

template <class...>
class Select;
template <class...>
class SelectFrom;
template <class...>
class SelectJoin;
template <class...>
class SelectJoinOn;
template <class...>
class SelectWhere;
template <class...>
class SelectOrderBy;
template <class...>
class SelectLimit;
template <class...>
class SelectQuery;

template <class... Params>
class Select {
    std::string sql_str;
    std::tuple<Params...> params;

public:
    explicit Select(std::string fields_str, Params&&... params);

    Select(const Select&) = delete;
    Select(Select&&) noexcept = default;
    Select& operator=(const Select&) = delete;
    Select& operator=(Select&&) = delete;
    ~Select() = default;

    SelectFrom<Params...> from(StringView table_name) &&;
};

template <class... Params>
Select(std::string, Params&&...) -> Select<Params...>;

template <class... Params>
class SelectFrom {
    std::string sql_str;
    std::tuple<Params...> params;

private:
    explicit SelectFrom(std::string sql_str, std::tuple<Params...> params);

public:
    SelectFrom(const SelectFrom&) = delete;
    SelectFrom(SelectFrom&&) noexcept = default;
    SelectFrom& operator=(const SelectFrom&) = delete;
    SelectFrom& operator=(SelectFrom&&) = delete;
    ~SelectFrom() = default;

    SelectJoin<Params...> join(StringView table_name) &&;

    template <template <class...> class T, class... OtherParams,
            std::enable_if_t<std::is_convertible_v<T<OtherParams...>, SelectQuery<OtherParams...>>,
                    int> = 0>
    SelectJoin<Params..., OtherParams...> join(
            T<OtherParams...> select_query, StringView table_name) &&;

    SelectJoin<Params...> left_join(StringView table_name) &&;

    template <template <class...> class T, class... OtherParams,
            std::enable_if_t<std::is_convertible_v<T<OtherParams...>, SelectQuery<OtherParams...>>,
                    int> = 0>
    SelectJoin<Params..., OtherParams...> left_join(
            T<OtherParams...> select_query, StringView table_name) &&;

    SelectJoin<Params...> inner_join(StringView table_name) &&;

    template <template <class...> class T, class... OtherParams,
            std::enable_if_t<std::is_convertible_v<T<OtherParams...>, SelectQuery<OtherParams...>>,
                    int> = 0>
    SelectJoin<Params..., OtherParams...> inner_join(
            T<OtherParams...> select_query, StringView table_name) &&;

    template <class... CondParams>
    SelectWhere<Params..., CondParams...> where(Condition<CondParams...> condition) &&;

    SelectWhere<Params...> where(StringView condition) &&;

    template <class CondParam>
    SelectWhere<Params..., CondParam> where(StringView condition, CondParam&& cond_param) &&;

    SelectOrderBy<Params...> order_by(StringView order_sql_str) &&;

    template <class... LimitParams>
    SelectLimit<Params..., LimitParams...> limit(
            std::string limit_sql_str, LimitParams&&... limit_params) &&;

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator SelectQuery<Params...>() &&;

    template <class...>
    friend class Select;
};

template <class... Params>
class SelectJoin {
    std::string sql_str;
    std::tuple<Params...> params;

private:
    explicit SelectJoin(std::string sql_str, std::tuple<Params...> params);

public:
    SelectJoin(const SelectJoin&) = delete;
    SelectJoin(SelectJoin&&) noexcept = default;
    SelectJoin& operator=(const SelectJoin&) = delete;
    SelectJoin& operator=(SelectJoin&&) = delete;
    ~SelectJoin() = default;

    template <class... CondParams>
    SelectJoinOn<Params..., CondParams...> on(Condition<CondParams...> condition) &&;

    SelectJoinOn<Params...> on(StringView condition) &&;

    template <class CondParam>
    SelectJoinOn<Params..., CondParam> on(StringView condition, CondParam&& cond_param) &&;

    SelectJoin<Params...> join(StringView table_name) &&;

    template <template <class...> class T, class... OtherParams,
            std::enable_if_t<std::is_convertible_v<T<OtherParams...>, SelectQuery<OtherParams...>>,
                    int> = 0>
    SelectJoin<Params..., OtherParams...> join(
            T<OtherParams...> select_query, StringView table_name) &&;

    SelectJoin<Params...> left_join(StringView table_name) &&;

    template <template <class...> class T, class... OtherParams,
            std::enable_if_t<std::is_convertible_v<T<OtherParams...>, SelectQuery<OtherParams...>>,
                    int> = 0>
    SelectJoin<Params..., OtherParams...> left_join(
            T<OtherParams...> select_query, StringView table_name) &&;

    SelectJoin<Params...> inner_join(StringView table_name) &&;

    template <template <class...> class T, class... OtherParams,
            std::enable_if_t<std::is_convertible_v<T<OtherParams...>, SelectQuery<OtherParams...>>,
                    int> = 0>
    SelectJoin<Params..., OtherParams...> inner_join(
            T<OtherParams...> select_query, StringView table_name) &&;

    template <class... CondParams>
    SelectWhere<Params..., CondParams...> where(Condition<CondParams...> condition) &&;

    SelectWhere<Params...> where(StringView condition) &&;

    template <class CondParam>
    SelectWhere<Params..., CondParam> where(StringView condition, CondParam&& cond_param) &&;

    SelectOrderBy<Params...> order_by(StringView order_sql_str) &&;

    template <class... LimitParams>
    SelectLimit<Params..., LimitParams...> limit(
            std::string limit_sql_str, LimitParams&&... limit_params) &&;

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator SelectQuery<Params...>() &&;

    template <class...>
    friend class SelectFrom;
    template <class...>
    friend class SelectJoinOn;
};

template <class... Params>
class SelectJoinOn {
    std::string sql_str;
    std::tuple<Params...> params;

private:
    explicit SelectJoinOn(std::string sql_str, std::tuple<Params...> params);

public:
    SelectJoinOn(const SelectJoinOn&) = delete;
    SelectJoinOn(SelectJoinOn&&) noexcept = default;
    SelectJoinOn& operator=(const SelectJoinOn&) = delete;
    SelectJoinOn& operator=(SelectJoinOn&&) = delete;
    ~SelectJoinOn() = default;

    SelectJoin<Params...> join(StringView table_name) &&;

    template <template <class...> class T, class... OtherParams,
            std::enable_if_t<std::is_convertible_v<T<OtherParams...>, SelectQuery<OtherParams...>>,
                    int> = 0>
    SelectJoin<Params..., OtherParams...> join(
            T<OtherParams...> select_query, StringView table_name) &&;

    SelectJoin<Params...> left_join(StringView table_name) &&;

    template <template <class...> class T, class... OtherParams,
            std::enable_if_t<std::is_convertible_v<T<OtherParams...>, SelectQuery<OtherParams...>>,
                    int> = 0>
    SelectJoin<Params..., OtherParams...> left_join(
            T<OtherParams...> select_query, StringView table_name) &&;

    SelectJoin<Params...> inner_join(StringView table_name) &&;

    template <template <class...> class T, class... OtherParams,
            std::enable_if_t<std::is_convertible_v<T<OtherParams...>, SelectQuery<OtherParams...>>,
                    int> = 0>
    SelectJoin<Params..., OtherParams...> inner_join(
            T<OtherParams...> select_query, StringView table_name) &&;

    template <class... CondParams>
    SelectWhere<Params..., CondParams...> where(Condition<CondParams...> condition) &&;

    SelectWhere<Params...> where(StringView condition) &&;

    template <class CondParam>
    SelectWhere<Params..., CondParam> where(StringView condition, CondParam&& cond_param) &&;

    SelectOrderBy<Params...> order_by(StringView order_sql_str) &&;

    template <class... LimitParams>
    SelectLimit<Params..., LimitParams...> limit(
            std::string limit_sql_str, LimitParams&&... limit_params) &&;

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator SelectQuery<Params...>() &&;

    template <class...>
    friend class SelectFrom;
    template <class...>
    friend class SelectJoin;
};

template <class... Params>
class SelectWhere {
    std::string sql_str;
    std::tuple<Params...> params;

private:
    explicit SelectWhere(std::string sql_str, std::tuple<Params...> params);

public:
    SelectWhere(const SelectWhere&) = delete;
    SelectWhere(SelectWhere&&) noexcept = default;
    SelectWhere& operator=(const SelectWhere&) = delete;
    SelectWhere& operator=(SelectWhere&&) = delete;
    ~SelectWhere() = default;

    SelectOrderBy<Params...> order_by(StringView order_sql_str) &&;

    template <class... LimitParams>
    SelectLimit<Params..., LimitParams...> limit(
            std::string limit_sql_str, LimitParams&&... limit_params) &&;

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator SelectQuery<Params...>() &&;

    template <class...>
    friend class SelectFrom;
    template <class...>
    friend class SelectJoin;
    template <class...>
    friend class SelectJoinOn;
};

template <class... Params>
class SelectOrderBy {
    std::string sql_str;
    std::tuple<Params...> params;

private:
    explicit SelectOrderBy(std::string sql_str, std::tuple<Params...> params);

public:
    SelectOrderBy(const SelectOrderBy&) = delete;
    SelectOrderBy(SelectOrderBy&&) noexcept = default;
    SelectOrderBy& operator=(const SelectOrderBy&) = delete;
    SelectOrderBy& operator=(SelectOrderBy&&) = delete;
    ~SelectOrderBy() = default;

    template <class... LimitParams>
    SelectLimit<Params..., LimitParams...> limit(
            std::string limit_sql_str, LimitParams&&... limit_params) &&;

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator SelectQuery<Params...>() &&;

    template <class...>
    friend class SelectFrom;
    template <class...>
    friend class SelectJoin;
    template <class...>
    friend class SelectJoinOn;
    template <class...>
    friend class SelectWhere;
};

template <class... Params>
class SelectLimit {
    std::string sql_str;
    std::tuple<Params...> params;

private:
    explicit SelectLimit(std::string sql_str, std::tuple<Params...> params);

public:
    SelectLimit(const SelectLimit&) = delete;
    SelectLimit(SelectLimit&&) noexcept = default;
    SelectLimit& operator=(const SelectLimit&) = delete;
    SelectLimit& operator=(SelectLimit&&) = delete;
    ~SelectLimit() = default;

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator SelectQuery<Params...>() &&;

    template <class...>
    friend class SelectFrom;
    template <class...>
    friend class SelectJoin;
    template <class...>
    friend class SelectJoinOn;
    template <class...>
    friend class SelectWhere;
    template <class...>
    friend class SelectOrderBy;
};

template <class... Params>
class SelectQuery {
public:
    const std::string sql_str;
    const std::tuple<Params...> params;

private:
    explicit SelectQuery(std::string sql_str, std::tuple<Params...> params);

public:
    SelectQuery(const SelectQuery&) = delete;
    SelectQuery(SelectQuery&&) noexcept = default;
    SelectQuery& operator=(const SelectQuery&) = delete;
    SelectQuery& operator=(SelectQuery&&) = delete;
    ~SelectQuery() = default;

    template <class...>
    friend class SelectFrom;
    template <class...>
    friend class SelectJoin;
    template <class...>
    friend class SelectJoinOn;
    template <class...>
    friend class SelectWhere;
    template <class...>
    friend class SelectOrderBy;
    template <class...>
    friend class SelectLimit;
};

/************************ Implementation ************************/

template <class... Params>
Condition<Params...>::Condition(std::string condition_sql_str, Params&&... params)
: sql_str{std::move(condition_sql_str)}
, params{std::forward<Params>(params)...} {
    assert(std::count(this->sql_str.begin(), this->sql_str.end(), '?') == sizeof...(params));
}

template <class... Params>
Condition<Params...>::Condition(std::string condition_sql_str, std::tuple<Params...> params)
: sql_str{std::move(condition_sql_str)}
, params{std::move(params)} {}

template <class... ParamsA, class... ParamsB>
Condition<ParamsA..., ParamsB...> operator and(Condition<ParamsA...> a, Condition<ParamsB...> b) {
    if constexpr (sizeof...(ParamsA) == 0) {
        if (a.sql_str == "TRUE") {
            return b;
        }
    }
    if constexpr (sizeof...(ParamsB) == 0) {
        if (b.sql_str == "TRUE") {
            return a;
        }
    }
    return {
            concat_tostr('(', std::move(a.sql_str), " AND ", std::move(b.sql_str), ')'),
            std::tuple_cat(std::move(a.params), std::move(b.params)),
    };
}

template <class... ParamsA, class... ParamsB>
Condition<ParamsA..., ParamsB...> operator or(Condition<ParamsA...> a, Condition<ParamsB...> b) {
    if constexpr (sizeof...(ParamsA) == 0) {
        if (a.sql_str == "FALSE") {
            return b;
        }
    }
    if constexpr (sizeof...(ParamsB) == 0) {
        if (b.sql_str == "FALSE") {
            return a;
        }
    }
    return {
            concat_tostr('(', std::move(a.sql_str), " OR ", std::move(b.sql_str), ')'),
            std::tuple_cat(std::move(a.params), std::move(b.params)),
    };
}

template <class... Params>
Select<Params...>::Select(std::string fields_str, Params&&... params)
: sql_str{concat_tostr("SELECT ", std::move(fields_str))}
, params{std::forward<Params>(params)...} {
    assert(std::count(this->sql_str.begin(), this->sql_str.end(), '?') == sizeof...(params));
}
template <class... Params>
SelectFrom<Params...> Select<Params...>::from(StringView table_name) && {
    return SelectFrom<Params...>{
            concat_tostr(std::move(sql_str), " FROM ", table_name), std::move(params)};
}

#define DEFINE_CONSTRUCTOR(ClassName)                                                  \
    template <class... Params> /* NOLINTNEXTLINE(bugprone-macro-parentheses) */        \
    ClassName<Params...>::ClassName(std::string sql_str, std::tuple<Params...> params) \
    : sql_str{std::move(sql_str)}                                                      \
    , params{std::move(params)} {}

#define DO_DEFINE_JOIN(ClassName, join_method_name, join_type_str)                          \
    template <class... Params> /* NOLINTNEXTLINE(bugprone-macro-parentheses) */             \
    SelectJoin<Params...> ClassName<Params...>::join_method_name(StringView table_name)&& { \
        return SelectJoin<Params...>{                                                       \
                concat_tostr(std::move(sql_str), " " join_type_str " ", table_name),        \
                std::move(params)};                                                         \
    }                                                                                       \
                                                                                            \
    template <class... Params>                                                              \
    template <template <class...> class T, class... OtherParams,                            \
            std::enable_if_t<                                                               \
                    std::is_convertible_v<T<OtherParams...>, SelectQuery<OtherParams...>>,  \
                    int>> /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                  \
    SelectJoin<Params..., OtherParams...> ClassName<Params...>::join_method_name(           \
            T<OtherParams...> select_query, StringView table_name)&& {                      \
        auto full_select_query = SelectQuery<OtherParams...>{std::move(select_query)};      \
        return SelectJoin<Params..., OtherParams...>{                                       \
                concat_tostr(std::move(sql_str), " " join_type_str " (",                    \
                        std::move(full_select_query.sql_str), ") ", table_name),            \
                std::tuple_cat(std::move(params), std::move(full_select_query.params))};    \
    }

#define DEFINE_JOIN(ClassName) DO_DEFINE_JOIN(ClassName, join, "JOIN")
#define DEFINE_LEFT_JOIN(ClassName) DO_DEFINE_JOIN(ClassName, left_join, "LEFT JOIN")
#define DEFINE_INNER_JOIN(ClassName) DO_DEFINE_JOIN(ClassName, inner_join, "INNER JOIN")

#define DEFINE_WHERE(ClassName)                                                         \
    template <class... Params>                                                          \
    template <class... CondParams> /* NOLINTNEXTLINE(bugprone-macro-parentheses) */     \
    SelectWhere<Params..., CondParams...> ClassName<Params...>::where(                  \
            Condition<CondParams...> condition)&& {                                     \
        return SelectWhere<Params..., CondParams...>{                                   \
                concat_tostr(std::move(sql_str), " WHERE ", condition.sql_str),         \
                std::tuple_cat(std::move(params), std::move(condition.params))};        \
    }                                                                                   \
                                                                                        \
    template <class... Params> /* NOLINTNEXTLINE(bugprone-macro-parentheses) */         \
    SelectWhere<Params...> ClassName<Params...>::where(StringView condition)&& {        \
        return std::move(*this).where(Condition{condition.to_string()});                \
    }                                                                                   \
                                                                                        \
    template <class... Params>                                                          \
    template <class CondParam> /* NOLINTNEXTLINE(bugprone-macro-parentheses) */         \
    SelectWhere<Params..., CondParam> ClassName<Params...>::where(                      \
            StringView condition, CondParam&& cond_param)&& {                           \
        return std::move(*this).where(                                                  \
                Condition{condition.to_string(), std::forward<CondParam>(cond_param)}); \
    }

#define DEFINE_ORDER_BY(ClassName)                                                                 \
    template <class... Params> /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                    \
    SelectOrderBy<Params...> ClassName<Params...>::order_by(StringView order_sql_str)&& {          \
        return SelectOrderBy<Params...>{                                                           \
                concat_tostr(std::move(sql_str), " ORDER BY ", order_sql_str), std::move(params)}; \
    }

#define DEFINE_LIMIT(ClassName)                                                                   \
    template <class... Params>                                                                    \
    template <class... LimitParams> /* NOLINTNEXTLINE(bugprone-macro-parentheses) */              \
    SelectLimit<Params..., LimitParams...> ClassName<Params...>::limit(                           \
            std::string limit_sql_str, LimitParams&&... limit_params)&& {                         \
        return SelectLimit<Params..., LimitParams...>{                                            \
                concat_tostr(std::move(sql_str), " LIMIT ", limit_sql_str),                       \
                std::tuple_cat(std::move(params),                                                 \
                        std::tuple<LimitParams...>{std::forward<LimitParams>(limit_params)...})}; \
    }

#define DEFINE_OPERATOR_SELECT_QUERY(ClassName)                                 \
    template <class... Params> /* NOLINTNEXTLINE(bugprone-macro-parentheses) */ \
    ClassName<Params...>::operator SelectQuery<Params...>()&& {                 \
        return SelectQuery<Params...>{std::move(sql_str), std::move(params)};   \
    }

DEFINE_CONSTRUCTOR(SelectFrom)
DEFINE_JOIN(SelectFrom)
DEFINE_LEFT_JOIN(SelectFrom)
DEFINE_INNER_JOIN(SelectFrom)
DEFINE_WHERE(SelectFrom)
DEFINE_ORDER_BY(SelectFrom)
DEFINE_LIMIT(SelectFrom)
DEFINE_OPERATOR_SELECT_QUERY(SelectFrom)

DEFINE_CONSTRUCTOR(SelectJoin)
DEFINE_JOIN(SelectJoin)
DEFINE_LEFT_JOIN(SelectJoin)
DEFINE_INNER_JOIN(SelectJoin)
DEFINE_WHERE(SelectJoin)
DEFINE_ORDER_BY(SelectJoin)
DEFINE_LIMIT(SelectJoin)
DEFINE_OPERATOR_SELECT_QUERY(SelectJoin)

template <class... Params>
template <class... CondParams>
SelectJoinOn<Params..., CondParams...> SelectJoin<Params...>::on(
        Condition<CondParams...> condition) && {
    return SelectJoinOn<Params..., CondParams...>{
            concat_tostr(std::move(sql_str), " ON ", condition.sql_str),
            std::tuple_cat(std::move(params), std::move(condition.params))};
}

template <class... Params> /* NOLINTNEXTLINE(bugprone-macro-parentheses) */
SelectJoinOn<Params...> SelectJoin<Params...>::on(StringView condition) && {
    return std::move(*this).on(Condition{condition.to_string()});
}

template <class... Params>
template <class CondParam> /* NOLINTNEXTLINE(bugprone-macro-parentheses) */
SelectJoinOn<Params..., CondParam> SelectJoin<Params...>::on(
        StringView condition, CondParam&& cond_param) && {
    return std::move(*this).on(
            Condition{condition.to_string(), std::forward<CondParam>(cond_param)});
}

DEFINE_CONSTRUCTOR(SelectJoinOn)
DEFINE_JOIN(SelectJoinOn)
DEFINE_LEFT_JOIN(SelectJoinOn)
DEFINE_INNER_JOIN(SelectJoinOn)
DEFINE_WHERE(SelectJoinOn)
DEFINE_ORDER_BY(SelectJoinOn)
DEFINE_LIMIT(SelectJoinOn)
DEFINE_OPERATOR_SELECT_QUERY(SelectJoinOn)

DEFINE_CONSTRUCTOR(SelectWhere)
DEFINE_ORDER_BY(SelectWhere)
DEFINE_LIMIT(SelectWhere)
DEFINE_OPERATOR_SELECT_QUERY(SelectWhere)

DEFINE_CONSTRUCTOR(SelectOrderBy)
DEFINE_LIMIT(SelectOrderBy)
DEFINE_OPERATOR_SELECT_QUERY(SelectOrderBy)

DEFINE_CONSTRUCTOR(SelectLimit)
DEFINE_OPERATOR_SELECT_QUERY(SelectLimit)

DEFINE_CONSTRUCTOR(SelectQuery)

} // namespace sql

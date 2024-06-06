#pragma once

#include <optional>
#include <simlib/meta/count.hh>
#include <simlib/throw_assert.hh>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace sim::sql {

template <class... Params>
class SqlWithParams {
    static_assert((std::is_reference_v<Params> && ... && true), "this is meant to hold references");
    std::string sql;
    std::tuple<Params...> params;

public:
    explicit SqlWithParams(std::string&& sql_, Params&&... params_)
    : sql{std::move(sql_)}
    , params{std::forward<Params>(params_)...} {
        throw_assert(meta::count(sql, '?') == sizeof...(Params));
    }

    explicit SqlWithParams(std::string&& sql_, std::tuple<Params...>&& params_)
    : sql{std::move(sql_)}
    , params{std::move(params_)} {
        throw_assert(meta::count(sql, '?') == sizeof...(Params));
    }

    ~SqlWithParams() = default;
    SqlWithParams(const SqlWithParams&) = delete;
    SqlWithParams(SqlWithParams&&) noexcept = default;
    SqlWithParams& operator=(const SqlWithParams&) = delete;
    SqlWithParams& operator=(SqlWithParams&&) = delete;

    std::string&& get_sql() && noexcept { return std::move(sql); }

    std::tuple<Params...>&& get_params() && noexcept { return std::move(params); }
};

template <class... Params>
SqlWithParams(std::string&&, Params&&...) -> SqlWithParams<Params&&...>;

template <class... Params>
class Select;
template <class... Params>
class SelectFrom;
template <class... Params>
class SelectJoin;
template <class... Params>
class SelectJoinOn;
template <class... Params>
class SelectWhere;
template <class... Params>
class SelectGroupBy;
template <class... Params>
class SelectOrderBy;
template <class... Params>
class SelectLimit;

template <class... Params>
class Condition;

class InsertInto;
template <class... Params>
class InsertValues;
template <class... Params>
class InsertSelect;
template <class... Params>
class InsertSelectFrom;
template <class... Params>
class InsertSelectWhere;
template <class... Params>
class InsertSelectGroupBy;
template <class... Params>
class InsertSelectOrderBy;

class Update;
template <class... Params>
class UpdateJoin;
template <class... Params>
class UpdateJoinOn;
template <class... Params>
class UpdateSet;
template <class... Params>
class UpdateWhere;

class DeleteFrom;
template <class... Params>
class DeleteWhere;

template <class... Params>
class Select {
    static_assert((std::is_reference_v<Params> && ... && true), "this is meant to hold references");

    std::string sql;
    std::tuple<Params...> params;

public:
    explicit Select(std::string&& sql_, Params&&... params_);

    ~Select() = default;
    Select(const Select&) = delete;
    Select(Select&&) = delete;
    Select& operator=(const Select&) = delete;
    Select& operator=(Select&&) = delete;

    SelectFrom<Params...> from(const std::string_view& sql_str) &&;
};

template <class... Params>
Select(std::string&&, Params&&...) -> Select<Params&&...>;

template <class... Params>
class SelectFrom {
    static_assert((std::is_reference_v<Params> && ... && true), "this is meant to hold references");

    std::string sql;
    std::tuple<Params...> params;

    template <class...>
    friend class Select;
    template <class...>
    friend class SelectJoinOn;
    friend class Update;
    template <class...>
    friend class UpdateJoinOn;

    explicit SelectFrom(std::string&& sql_, std::tuple<Params...>&& params_) noexcept
    : sql{std::move(sql_)}
    , params{std::move(params_)} {}

public:
    ~SelectFrom() = default;
    SelectFrom(const SelectFrom&) = delete;
    SelectFrom(SelectFrom&&) = delete;
    SelectFrom& operator=(const SelectFrom&) = delete;
    SelectFrom& operator=(SelectFrom&&) = delete;

    SelectJoin<Params...> left_join(const std::string_view& sql_str) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    left_join(SelectFrom<SelectParams...>&& select_from, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    left_join(SelectJoinOn<SelectParams...>&& select_join_on, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    left_join(SelectWhere<SelectParams...>&& select_where, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    left_join(SelectGroupBy<SelectParams...>&& select_group_by, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    left_join(SelectOrderBy<SelectParams...>&& select_order_by, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    left_join(SelectLimit<SelectParams...>&& select_limit, std::string_view table_name) &&;

    SelectJoin<Params...> right_join(const std::string_view& sql_str) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    right_join(SelectFrom<SelectParams...>&& select_from, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    right_join(SelectJoinOn<SelectParams...>&& select_join_on, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    right_join(SelectWhere<SelectParams...>&& select_where, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    right_join(SelectGroupBy<SelectParams...>&& select_group_by, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    right_join(SelectOrderBy<SelectParams...>&& select_order_by, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    right_join(SelectLimit<SelectParams...>&& select_limit, std::string_view table_name) &&;

    SelectJoin<Params...> inner_join(const std::string_view& sql_str) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    inner_join(SelectFrom<SelectParams...>&& select_from, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    inner_join(SelectJoinOn<SelectParams...>&& select_join_on, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    inner_join(SelectWhere<SelectParams...>&& select_where, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    inner_join(SelectGroupBy<SelectParams...>&& select_group_by, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    inner_join(SelectOrderBy<SelectParams...>&& select_order_by, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    inner_join(SelectLimit<SelectParams...>&& select_limit, std::string_view table_name) &&;

    template <class... WhereParams>
    SelectWhere<Params..., WhereParams&&...>
    where(const std::string_view& sql_str, WhereParams&&... where_params) &&;

    template <class... CondParams>
    SelectWhere<Params..., CondParams...> where(Condition<CondParams...>&& condition) &&;

    SelectGroupBy<Params...> group_by(const std::string_view& sql_str) &&;

    SelectOrderBy<Params...> order_by(const std::string_view& sql_str) &&;

    template <class... LimitParams>
    SelectLimit<Params..., LimitParams&&...>
    limit(const std::string_view& sql_str, LimitParams&&... limit_params) &&;

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator SqlWithParams<Params...>() && noexcept;
};

template <class... Params>
class SelectJoin {
    static_assert((std::is_reference_v<Params> && ... && true), "this is meant to hold references");

    std::string sql;
    std::tuple<Params...> params;

    template <class...>
    friend class SelectFrom;
    template <class...>
    friend class SelectJoinOn;

    explicit SelectJoin(std::string&& sql_, std::tuple<Params...>&& params_) noexcept
    : sql{std::move(sql_)}
    , params{std::move(params_)} {}

public:
    ~SelectJoin() = default;
    SelectJoin(const SelectJoin&) = delete;
    SelectJoin(SelectJoin&&) = delete;
    SelectJoin& operator=(const SelectJoin&) = delete;
    SelectJoin& operator=(SelectJoin&&) = delete;

    template <class... OnParams>
    SelectJoinOn<Params..., OnParams&&...>
    on(const std::string_view& sql_str, OnParams&&... on_params) &&;

    template <class... CondParams>
    SelectJoinOn<Params..., CondParams...> on(Condition<CondParams...>&& condition) &&;
};

template <class... Params>
class SelectJoinOn {
    static_assert((std::is_reference_v<Params> && ... && true), "this is meant to hold references");

    std::string sql;
    std::tuple<Params...> params;

    template <class...>
    friend class SelectFrom;
    template <class...>
    friend class SelectJoin;
    friend class Update;
    template <class...>
    friend class UpdateJoinOn;

    explicit SelectJoinOn(std::string&& sql_, std::tuple<Params...>&& params_) noexcept
    : sql{std::move(sql_)}
    , params{std::move(params_)} {}

public:
    ~SelectJoinOn() = default;
    SelectJoinOn(const SelectJoinOn&) = delete;
    SelectJoinOn(SelectJoinOn&&) = delete;
    SelectJoinOn& operator=(const SelectJoinOn&) = delete;
    SelectJoinOn& operator=(SelectJoinOn&&) = delete;

    SelectJoin<Params...> left_join(const std::string_view& sql_str) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    left_join(SelectFrom<SelectParams...>&& select_from, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    left_join(SelectJoinOn<SelectParams...>&& select_join_on, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    left_join(SelectWhere<SelectParams...>&& select_where, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    left_join(SelectGroupBy<SelectParams...>&& select_group_by, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    left_join(SelectOrderBy<SelectParams...>&& select_order_by, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    left_join(SelectLimit<SelectParams...>&& select_limit, std::string_view table_name) &&;

    SelectJoin<Params...> right_join(const std::string_view& sql_str) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    right_join(SelectFrom<SelectParams...>&& select_from, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    right_join(SelectJoinOn<SelectParams...>&& select_join_on, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    right_join(SelectWhere<SelectParams...>&& select_where, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    right_join(SelectGroupBy<SelectParams...>&& select_group_by, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    right_join(SelectOrderBy<SelectParams...>&& select_order_by, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    right_join(SelectLimit<SelectParams...>&& select_limit, std::string_view table_name) &&;

    SelectJoin<Params...> inner_join(const std::string_view& sql_str) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    inner_join(SelectFrom<SelectParams...>&& select_from, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    inner_join(SelectJoinOn<SelectParams...>&& select_join_on, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    inner_join(SelectWhere<SelectParams...>&& select_where, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    inner_join(SelectGroupBy<SelectParams...>&& select_group_by, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    inner_join(SelectOrderBy<SelectParams...>&& select_order_by, std::string_view table_name) &&;

    template <class... SelectParams>
    SelectJoin<Params..., SelectParams...>
    inner_join(SelectLimit<SelectParams...>&& select_limit, std::string_view table_name) &&;

    template <class... WhereParams>
    SelectWhere<Params..., WhereParams&&...>
    where(const std::string_view& sql_str, WhereParams&&... where_params) &&;

    template <class... CondParams>
    SelectWhere<Params..., CondParams...> where(Condition<CondParams...>&& condition) &&;

    SelectGroupBy<Params...> group_by(const std::string_view& sql_str) &&;

    SelectOrderBy<Params...> order_by(const std::string_view& sql_str) &&;

    template <class... LimitParams>
    SelectLimit<Params..., LimitParams&&...>
    limit(const std::string_view& sql_str, LimitParams&&... limit_params) &&;

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator SqlWithParams<Params...>() && noexcept;
};

template <class... Params>
class SelectWhere {
    static_assert((std::is_reference_v<Params> && ... && true), "this is meant to hold references");

    std::string sql;
    std::tuple<Params...> params;

    template <class...>
    friend class SelectFrom;
    template <class...>
    friend class SelectJoinOn;
    friend class Update;
    template <class...>
    friend class UpdateJoinOn;

    explicit SelectWhere(std::string&& sql_, std::tuple<Params...>&& params_) noexcept
    : sql{std::move(sql_)}
    , params{std::move(params_)} {}

public:
    ~SelectWhere() = default;
    SelectWhere(const SelectWhere&) = delete;
    SelectWhere(SelectWhere&&) = delete;
    SelectWhere& operator=(const SelectWhere&) = delete;
    SelectWhere& operator=(SelectWhere&&) = delete;

    SelectGroupBy<Params...> group_by(const std::string_view& sql_str) &&;

    SelectOrderBy<Params...> order_by(const std::string_view& sql_str) &&;

    template <class... LimitParams>
    SelectLimit<Params..., LimitParams&&...>
    limit(const std::string_view& sql_str, LimitParams&&... limit_params) &&;

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator SqlWithParams<Params...>() && noexcept;
};

template <class... Params>
class SelectGroupBy {
    static_assert((std::is_reference_v<Params> && ... && true), "this is meant to hold references");

    std::string sql;
    std::tuple<Params...> params;

    template <class...>
    friend class SelectFrom;
    template <class...>
    friend class SelectJoinOn;
    template <class...>
    friend class SelectWhere;
    friend class Update;
    template <class...>
    friend class UpdateJoinOn;

    explicit SelectGroupBy(std::string&& sql_, std::tuple<Params...>&& params_) noexcept
    : sql{std::move(sql_)}
    , params{std::move(params_)} {}

public:
    ~SelectGroupBy() = default;
    SelectGroupBy(const SelectGroupBy&) = delete;
    SelectGroupBy(SelectGroupBy&&) = delete;
    SelectGroupBy& operator=(const SelectGroupBy&) = delete;
    SelectGroupBy& operator=(SelectGroupBy&&) = delete;

    SelectOrderBy<Params...> order_by(const std::string_view& sql_str) &&;

    template <class... LimitParams>
    SelectLimit<Params..., LimitParams&&...>
    limit(const std::string_view& sql_str, LimitParams&&... limit_params) &&;

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator SqlWithParams<Params...>() && noexcept;
};

template <class... Params>
class SelectOrderBy {
    static_assert((std::is_reference_v<Params> && ... && true), "this is meant to hold references");

    std::string sql;
    std::tuple<Params...> params;

    template <class...>
    friend class SelectFrom;
    template <class...>
    friend class SelectJoinOn;
    template <class...>
    friend class SelectWhere;
    template <class...>
    friend class SelectGroupBy;
    friend class Update;
    template <class...>
    friend class UpdateJoinOn;

    explicit SelectOrderBy(std::string&& sql_, std::tuple<Params...>&& params_) noexcept
    : sql{std::move(sql_)}
    , params{std::move(params_)} {}

public:
    ~SelectOrderBy() = default;
    SelectOrderBy(const SelectOrderBy&) = delete;
    SelectOrderBy(SelectOrderBy&&) = delete;
    SelectOrderBy& operator=(const SelectOrderBy&) = delete;
    SelectOrderBy& operator=(SelectOrderBy&&) = delete;

    template <class... LimitParams>
    SelectLimit<Params..., LimitParams&&...>
    limit(const std::string_view& sql_str, LimitParams&&... limit_params) &&;

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator SqlWithParams<Params...>() && noexcept;
};

template <class... Params>
class SelectLimit {
    static_assert((std::is_reference_v<Params> && ... && true), "this is meant to hold references");

    std::string sql;
    std::tuple<Params...> params;

    template <class...>
    friend class SelectFrom;
    template <class...>
    friend class SelectJoinOn;
    template <class...>
    friend class SelectWhere;
    template <class...>
    friend class SelectGroupBy;
    template <class...>
    friend class SelectOrderBy;
    friend class Update;
    template <class...>
    friend class UpdateJoinOn;

    explicit SelectLimit(std::string&& sql_, std::tuple<Params...>&& params_) noexcept
    : sql{std::move(sql_)}
    , params{std::move(params_)} {}

public:
    ~SelectLimit() = default;
    SelectLimit(const SelectLimit&) = delete;
    SelectLimit(SelectLimit&&) = delete;
    SelectLimit& operator=(const SelectLimit&) = delete;
    SelectLimit& operator=(SelectLimit&&) = delete;

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator SqlWithParams<Params...>() && noexcept;
};

template <class... Params>
class Condition {
    static_assert((std::is_reference_v<Params> && ... && true), "this is meant to hold references");

    std::string sql;
    std::tuple<Params...> params;

    template <class...>
    friend class SelectFrom;
    template <class...>
    friend class SelectJoin;
    template <class...>
    friend class SelectJoinOn;
    template <class...>
    friend class UpdateJoin;

    explicit Condition(std::string&& sql_, std::tuple<Params...>&& params_) noexcept
    : sql{std::move(sql_)}
    , params{std::move(params_)} {}

public:
    explicit Condition(std::string&& sql_, Params&&... params_);

    ~Condition() = default;
    Condition(const Condition&) = default;
    Condition(Condition&&) = default;
    Condition& operator=(const Condition&) = delete;
    Condition& operator=(Condition&&) = default;

    template <class... Params1, class... Params2>
    Condition<Params1..., Params2...> friend
    operator&&(Condition<Params1...>&& cond1, Condition<Params2...>&& cond2);

    template <class... Params1, class... Params2>
    Condition<Params1..., Params2...> friend
    operator||(Condition<Params1...>&& cond1, Condition<Params2...>&& cond2);
};

template <class... Params>
Condition(std::string&&, Params&&...) -> Condition<Params&&...>;

class Update {
    std::string sql;

public:
    explicit Update(std::string&& sql_);

    ~Update() = default;
    Update(const Update&) = delete;
    Update(Update&&) = delete;
    Update& operator=(const Update&) = delete;
    Update& operator=(Update&&) = delete;

    template <class... SetParams>
    UpdateSet<SetParams&&...> set(const std::string_view& sql_str, SetParams&&... set_params) &&;

    UpdateJoin<> left_join(const std::string_view& sql_str) &&;

    template <class... SelectParams>
    UpdateJoin<SelectParams...>
    left_join(SelectFrom<SelectParams...>&& select_from, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<SelectParams...>
    left_join(SelectJoinOn<SelectParams...>&& select_join_on, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<SelectParams...>
    left_join(SelectWhere<SelectParams...>&& select_where, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<SelectParams...>
    left_join(SelectGroupBy<SelectParams...>&& select_group_by, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<SelectParams...>
    left_join(SelectOrderBy<SelectParams...>&& select_order_by, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<SelectParams...>
    left_join(SelectLimit<SelectParams...>&& select_limit, std::string_view table_name) &&;

    UpdateJoin<> right_join(const std::string_view& sql_str) &&;

    template <class... SelectParams>
    UpdateJoin<SelectParams...>
    right_join(SelectFrom<SelectParams...>&& select_from, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<SelectParams...>
    right_join(SelectJoinOn<SelectParams...>&& select_join_on, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<SelectParams...>
    right_join(SelectWhere<SelectParams...>&& select_where, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<SelectParams...>
    right_join(SelectGroupBy<SelectParams...>&& select_group_by, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<SelectParams...>
    right_join(SelectOrderBy<SelectParams...>&& select_order_by, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<SelectParams...>
    right_join(SelectLimit<SelectParams...>&& select_limit, std::string_view table_name) &&;

    UpdateJoin<> inner_join(const std::string_view& sql_str) &&;

    template <class... SelectParams>
    UpdateJoin<SelectParams...>
    inner_join(SelectFrom<SelectParams...>&& select_from, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<SelectParams...>
    inner_join(SelectJoinOn<SelectParams...>&& select_join_on, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<SelectParams...>
    inner_join(SelectWhere<SelectParams...>&& select_where, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<SelectParams...>
    inner_join(SelectGroupBy<SelectParams...>&& select_group_by, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<SelectParams...>
    inner_join(SelectOrderBy<SelectParams...>&& select_order_by, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<SelectParams...>
    inner_join(SelectLimit<SelectParams...>&& select_limit, std::string_view table_name) &&;
};

template <class... Params>
class UpdateJoin {
    static_assert((std::is_reference_v<Params> && ... && true), "this is meant to hold references");

    std::string sql;
    std::tuple<Params...> params;

    friend class Update;
    template <class...>
    friend class UpdateJoinOn;

    explicit UpdateJoin(std::string&& sql_, std::tuple<Params...>&& params_) noexcept
    : sql{std::move(sql_)}
    , params{std::move(params_)} {}

public:
    ~UpdateJoin() = default;
    UpdateJoin(const UpdateJoin&) = delete;
    UpdateJoin(UpdateJoin&&) = delete;
    UpdateJoin& operator=(const UpdateJoin&) = delete;
    UpdateJoin& operator=(UpdateJoin&&) = delete;

    template <class... OnParams>
    UpdateJoinOn<Params..., OnParams&&...>
    on(const std::string_view& sql_str, OnParams&&... on_params) &&;

    template <class... CondParams>
    UpdateJoinOn<Params..., CondParams...> on(Condition<CondParams...>&& condition) &&;
};

template <class... Params>
class UpdateJoinOn {
    static_assert((std::is_reference_v<Params> && ... && true), "this is meant to hold references");

    std::string sql;
    std::tuple<Params...> params;

    template <class...>
    friend class UpdateJoin;

    explicit UpdateJoinOn(std::string&& sql_, std::tuple<Params...>&& params_) noexcept
    : sql{std::move(sql_)}
    , params{std::move(params_)} {}

public:
    ~UpdateJoinOn() = default;
    UpdateJoinOn(const UpdateJoinOn&) = delete;
    UpdateJoinOn(UpdateJoinOn&&) = delete;
    UpdateJoinOn& operator=(const UpdateJoinOn&) = delete;
    UpdateJoinOn& operator=(UpdateJoinOn&&) = delete;

    template <class... SetParams>
    UpdateSet<Params..., SetParams&&...>
    set(const std::string_view& sql_str, SetParams&&... set_params) &&;

    UpdateJoin<Params...> left_join(const std::string_view& sql_str) &&;

    template <class... SelectParams>
    UpdateJoin<Params..., SelectParams...>
    left_join(SelectFrom<SelectParams...>&& select_from, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<Params..., SelectParams...>
    left_join(SelectJoinOn<SelectParams...>&& select_join_on, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<Params..., SelectParams...>
    left_join(SelectWhere<SelectParams...>&& select_where, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<Params..., SelectParams...>
    left_join(SelectGroupBy<SelectParams...>&& select_group_by, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<Params..., SelectParams...>
    left_join(SelectOrderBy<SelectParams...>&& select_order_by, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<Params..., SelectParams...>
    left_join(SelectLimit<SelectParams...>&& select_limit, std::string_view table_name) &&;

    UpdateJoin<Params...> right_join(const std::string_view& sql_str) &&;

    template <class... SelectParams>
    UpdateJoin<Params..., SelectParams...>
    right_join(SelectFrom<SelectParams...>&& select_from, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<Params..., SelectParams...>
    right_join(SelectJoinOn<SelectParams...>&& select_join_on, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<Params..., SelectParams...>
    right_join(SelectWhere<SelectParams...>&& select_where, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<Params..., SelectParams...>
    right_join(SelectGroupBy<SelectParams...>&& select_group_by, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<Params..., SelectParams...>
    right_join(SelectOrderBy<SelectParams...>&& select_order_by, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<Params..., SelectParams...>
    right_join(SelectLimit<SelectParams...>&& select_limit, std::string_view table_name) &&;

    UpdateJoin<Params...> inner_join(const std::string_view& sql_str) &&;

    template <class... SelectParams>
    UpdateJoin<Params..., SelectParams...>
    inner_join(SelectFrom<SelectParams...>&& select_from, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<Params..., SelectParams...>
    inner_join(SelectJoinOn<SelectParams...>&& select_join_on, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<Params..., SelectParams...>
    inner_join(SelectWhere<SelectParams...>&& select_where, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<Params..., SelectParams...>
    inner_join(SelectGroupBy<SelectParams...>&& select_group_by, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<Params..., SelectParams...>
    inner_join(SelectOrderBy<SelectParams...>&& select_order_by, std::string_view table_name) &&;

    template <class... SelectParams>
    UpdateJoin<Params..., SelectParams...>
    inner_join(SelectLimit<SelectParams...>&& select_limit, std::string_view table_name) &&;
};

template <class... Params>
class UpdateSet {
    static_assert((std::is_reference_v<Params> && ... && true), "this is meant to hold references");

    std::string sql;
    std::tuple<Params...> params;

    friend class Update;
    template <class...>
    friend class UpdateJoinOn;

    explicit UpdateSet(std::string&& sql_, std::tuple<Params...>&& params_) noexcept
    : sql{std::move(sql_)}
    , params{std::move(params_)} {}

public:
    ~UpdateSet() = default;
    UpdateSet(const UpdateSet&) = delete;
    UpdateSet(UpdateSet&&) = delete;
    UpdateSet& operator=(const UpdateSet&) = delete;
    UpdateSet& operator=(UpdateSet&&) = delete;

    template <class... WhereParams>
    UpdateWhere<Params..., WhereParams&&...>
    where(const std::string_view& sql_str, WhereParams&&... where_params) &&;

    template <class... CondParams>
    UpdateWhere<Params..., CondParams...> where(Condition<CondParams...>&& condition) &&;

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator SqlWithParams<Params...>() && noexcept;
};

template <class... Params>
class UpdateWhere {
    static_assert((std::is_reference_v<Params> && ... && true), "this is meant to hold references");

    std::string sql;
    std::tuple<Params...> params;

    template <class...>
    friend class UpdateSet;

    explicit UpdateWhere(std::string&& sql_, std::tuple<Params...>&& params_) noexcept
    : sql{std::move(sql_)}
    , params{std::move(params_)} {}

public:
    ~UpdateWhere() = default;
    UpdateWhere(const UpdateWhere&) = delete;
    UpdateWhere(UpdateWhere&&) = delete;
    UpdateWhere& operator=(const UpdateWhere&) = delete;
    UpdateWhere& operator=(UpdateWhere&&) = delete;

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator SqlWithParams<Params...>() && noexcept;
};

class DeleteFrom {
    std::string sql;

public:
    explicit DeleteFrom(std::string&& sql_);

    ~DeleteFrom() = default;
    DeleteFrom(const DeleteFrom&) = delete;
    DeleteFrom(DeleteFrom&&) = delete;
    DeleteFrom& operator=(const DeleteFrom&) = delete;
    DeleteFrom& operator=(DeleteFrom&&) = delete;

    template <class... WhereParams>
    DeleteWhere<WhereParams&&...>
    where(const std::string_view& sql_str, WhereParams&&... where_params) &&;

    template <class... CondParams>
    DeleteWhere<CondParams...> where(Condition<CondParams...>&& condition) &&;

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator SqlWithParams<>() && noexcept;
};

template <class... Params>
class DeleteWhere {
    static_assert((std::is_reference_v<Params> && ... && true), "this is meant to hold references");

    std::string sql;
    std::tuple<Params...> params;

    friend class DeleteFrom;

    explicit DeleteWhere(std::string&& sql_, std::tuple<Params...>&& params_) noexcept
    : sql{std::move(sql_)}
    , params{std::move(params_)} {}

public:
    ~DeleteWhere() = default;
    DeleteWhere(const DeleteWhere&) = delete;
    DeleteWhere(DeleteWhere&&) = delete;
    DeleteWhere& operator=(const DeleteWhere&) = delete;
    DeleteWhere& operator=(DeleteWhere&&) = delete;

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator SqlWithParams<Params...>() && noexcept;
};

class InsertInto {
    std::string sql;

public:
    explicit InsertInto(std::string&& sql_);

    ~InsertInto() = default;
    InsertInto(const InsertInto&) = delete;
    InsertInto(InsertInto&&) = delete;
    InsertInto& operator=(const InsertInto&) = delete;
    InsertInto& operator=(InsertInto&&) = delete;

    template <class... ValuesParams>
    InsertValues<ValuesParams&&...>
    values(const std::string_view& sql_str, ValuesParams&&... values_params) &&;

    template <class... SelectParams>
    InsertSelect<SelectParams&&...>
    select(const std::string_view& sql_str, SelectParams&&... select_params) &&;
};

class InsertIgnoreInto {
    std::string sql;

public:
    explicit InsertIgnoreInto(std::string&& sql_);

    ~InsertIgnoreInto() = default;
    InsertIgnoreInto(const InsertIgnoreInto&) = delete;
    InsertIgnoreInto(InsertIgnoreInto&&) = delete;
    InsertIgnoreInto& operator=(const InsertIgnoreInto&) = delete;
    InsertIgnoreInto& operator=(InsertIgnoreInto&&) = delete;

    template <class... ValuesParams>
    InsertValues<ValuesParams&&...>
    values(const std::string_view& sql_str, ValuesParams&&... values_params) &&;

    template <class... SelectParams>
    InsertSelect<SelectParams&&...>
    select(const std::string_view& sql_str, SelectParams&&... select_params) &&;
};

template <class... Params>
class InsertValues {
    static_assert((std::is_reference_v<Params> && ... && true), "this is meant to hold references");

    std::string sql;
    std::tuple<Params...> params;

    friend class InsertInto;
    friend class InsertIgnoreInto;

    explicit InsertValues(std::string&& sql_, std::tuple<Params...>&& params_) noexcept
    : sql{std::move(sql_)}
    , params{std::move(params_)} {}

public:
    ~InsertValues() = default;
    InsertValues(const InsertValues&) = delete;
    InsertValues(InsertValues&&) = delete;
    InsertValues& operator=(const InsertValues&) = delete;
    InsertValues& operator=(InsertValues&&) = delete;

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator SqlWithParams<Params...>() && noexcept;
};

template <class... Params>
class InsertSelect {
    static_assert((std::is_reference_v<Params> && ... && true), "this is meant to hold references");

    std::string sql;
    std::tuple<Params...> params;

    friend class InsertInto;
    friend class InsertIgnoreInto;

    explicit InsertSelect(std::string&& sql_, std::tuple<Params...>&& params_) noexcept
    : sql{std::move(sql_)}
    , params{std::move(params_)} {}

public:
    ~InsertSelect() = default;
    InsertSelect(const InsertSelect&) = delete;
    InsertSelect(InsertSelect&&) = delete;
    InsertSelect& operator=(const InsertSelect&) = delete;
    InsertSelect& operator=(InsertSelect&&) = delete;

    InsertSelectFrom<Params...> from(const std::string_view& sql_str) &&;
};

template <class... Params>
class InsertSelectFrom {
    static_assert((std::is_reference_v<Params> && ... && true), "this is meant to hold references");

    std::string sql;
    std::tuple<Params...> params;

    template <class...>
    friend class InsertSelect;

    explicit InsertSelectFrom(std::string&& sql_, std::tuple<Params...>&& params_) noexcept
    : sql{std::move(sql_)}
    , params{std::move(params_)} {}

public:
    ~InsertSelectFrom() = default;
    InsertSelectFrom(const InsertSelectFrom&) = delete;
    InsertSelectFrom(InsertSelectFrom&&) = delete;
    InsertSelectFrom& operator=(const InsertSelectFrom&) = delete;
    InsertSelectFrom& operator=(InsertSelectFrom&&) = delete;

    template <class... WhereParams>
    InsertSelectWhere<Params..., WhereParams&&...>
    where(const std::string_view& sql_str, WhereParams&&... where_params) &&;

    template <class... CondParams>
    InsertSelectWhere<Params..., CondParams...> where(Condition<CondParams...>&& condition) &&;

    InsertSelectGroupBy<Params...> group_by(const std::string_view& sql_str) &&;

    InsertSelectOrderBy<Params...> order_by(const std::string_view& sql_str) &&;

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator SqlWithParams<Params...>() && noexcept;
};

template <class... Params>
class InsertSelectWhere {
    static_assert((std::is_reference_v<Params> && ... && true), "this is meant to hold references");

    std::string sql;
    std::tuple<Params...> params;

    template <class...>
    friend class InsertSelectFrom;

    explicit InsertSelectWhere(std::string&& sql_, std::tuple<Params...>&& params_) noexcept
    : sql{std::move(sql_)}
    , params{std::move(params_)} {}

public:
    ~InsertSelectWhere() = default;
    InsertSelectWhere(const InsertSelectWhere&) = delete;
    InsertSelectWhere(InsertSelectWhere&&) = delete;
    InsertSelectWhere& operator=(const InsertSelectWhere&) = delete;
    InsertSelectWhere& operator=(InsertSelectWhere&&) = delete;

    InsertSelectGroupBy<Params...> group_by(const std::string_view& sql_str) &&;

    InsertSelectOrderBy<Params...> order_by(const std::string_view& sql_str) &&;

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator SqlWithParams<Params...>() && noexcept;
};

template <class... Params>
class InsertSelectGroupBy {
    static_assert((std::is_reference_v<Params> && ... && true), "this is meant to hold references");

    std::string sql;
    std::tuple<Params...> params;

    template <class...>
    friend class InsertSelectFrom;
    template <class...>
    friend class InsertSelectWhere;

    explicit InsertSelectGroupBy(std::string&& sql_, std::tuple<Params...>&& params_) noexcept
    : sql{std::move(sql_)}
    , params{std::move(params_)} {}

public:
    ~InsertSelectGroupBy() = default;
    InsertSelectGroupBy(const InsertSelectGroupBy&) = delete;
    InsertSelectGroupBy(InsertSelectGroupBy&&) = delete;
    InsertSelectGroupBy& operator=(const InsertSelectGroupBy&) = delete;
    InsertSelectGroupBy& operator=(InsertSelectGroupBy&&) = delete;

    InsertSelectOrderBy<Params...> order_by(const std::string_view& sql_str) &&;

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator SqlWithParams<Params...>() && noexcept;
};

template <class... Params>
class InsertSelectOrderBy {
    static_assert((std::is_reference_v<Params> && ... && true), "this is meant to hold references");

    std::string sql;
    std::tuple<Params...> params;

    template <class...>
    friend class InsertSelectFrom;
    template <class...>
    friend class InsertSelectWhere;
    template <class...>
    friend class InsertSelectGroupBy;

    explicit InsertSelectOrderBy(std::string&& sql_, std::tuple<Params...>&& params_) noexcept
    : sql{std::move(sql_)}
    , params{std::move(params_)} {}

public:
    ~InsertSelectOrderBy() = default;
    InsertSelectOrderBy(const InsertSelectOrderBy&) = delete;
    InsertSelectOrderBy(InsertSelectOrderBy&&) = delete;
    InsertSelectOrderBy& operator=(const InsertSelectOrderBy&) = delete;
    InsertSelectOrderBy& operator=(InsertSelectOrderBy&&) = delete;

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator SqlWithParams<Params...>() && noexcept;
};

/***************************************** IMPLEMENTATION *****************************************/

// Select

template <class... Params>
Select<Params...>::Select(std::string&& sql_, Params&&... params_)
: sql{"SELECT " + std::move(sql_)}
, params{std::forward<Params>(params_)...} {
    throw_assert(meta::count(sql, '?') == sizeof...(Params));
}

#define DEFINE_FROM(ResultClass, Class)                                                \
    template <class... Params> /* NOLINTNEXTLINE(bugprone-macro-parentheses) */        \
    ResultClass<Params...> Class<Params...>::from(const std::string_view& sql_str)&& { \
        throw_assert(meta::count(sql_str, '?') == 0);                                  \
        sql += " FROM ";                                                               \
        sql += sql_str;                                                                \
        /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                               \
        return ResultClass<Params...>{std::move(sql), std::move(params)};              \
    }

DEFINE_FROM(SelectFrom, Select)

// SelectFrom

#define DEFINE_JOIN(ResultClass, Class, join_name, join_str)                                \
    template <class... Params> /* NOLINTNEXTLINE(bugprone-macro-parentheses) */             \
    ResultClass<Params...> Class<Params...>::join_name(const std::string_view& sql_str)&& { \
        throw_assert(meta::count(sql_str, '?') == 0);                                       \
        sql += " " join_str " ";                                                            \
        sql += sql_str;                                                                     \
        /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                                    \
        return ResultClass<Params...>{std::move(sql), std::move(params)};                   \
    }

#define DEFINE_JOIN_SELECT(ResultClass, Class, join_name, join_str, SelectType)         \
    template <class... Params>                                                          \
    template <class... SelectParams> /* NOLINTNEXTLINE(bugprone-macro-parentheses) */   \
    ResultClass<Params..., SelectParams...> Class<Params...>::join_name(                \
        SelectType<SelectParams...>&& select, /* NOLINT(bugprone-macro-parentheses) */  \
        std::string_view table_name                                                     \
    )&& {                                                                               \
        throw_assert(meta::count(table_name, '?') == 0);                                \
        sql += " " join_str " (";                                                       \
        sql += std::move(select).sql;                                                   \
        sql += ") ";                                                                    \
        sql += table_name;                                                              \
        /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                                \
        return ResultClass<Params..., SelectParams...>{                                 \
            std::move(sql), std::tuple_cat(std::move(params), std::move(select).params) \
        };                                                                              \
    }

DEFINE_JOIN(SelectJoin, SelectFrom, left_join, "LEFT JOIN")
DEFINE_JOIN_SELECT(SelectJoin, SelectFrom, left_join, "LEFT JOIN", SelectFrom)
DEFINE_JOIN_SELECT(SelectJoin, SelectFrom, left_join, "LEFT JOIN", SelectJoinOn)
DEFINE_JOIN_SELECT(SelectJoin, SelectFrom, left_join, "LEFT JOIN", SelectWhere)
DEFINE_JOIN_SELECT(SelectJoin, SelectFrom, left_join, "LEFT JOIN", SelectGroupBy)
DEFINE_JOIN_SELECT(SelectJoin, SelectFrom, left_join, "LEFT JOIN", SelectOrderBy)
DEFINE_JOIN_SELECT(SelectJoin, SelectFrom, left_join, "LEFT JOIN", SelectLimit)

DEFINE_JOIN(SelectJoin, SelectFrom, right_join, "RIGHT JOIN")
DEFINE_JOIN_SELECT(SelectJoin, SelectFrom, right_join, "RIGHT JOIN", SelectFrom)
DEFINE_JOIN_SELECT(SelectJoin, SelectFrom, right_join, "RIGHT JOIN", SelectJoinOn)
DEFINE_JOIN_SELECT(SelectJoin, SelectFrom, right_join, "RIGHT JOIN", SelectWhere)
DEFINE_JOIN_SELECT(SelectJoin, SelectFrom, right_join, "RIGHT JOIN", SelectGroupBy)
DEFINE_JOIN_SELECT(SelectJoin, SelectFrom, right_join, "RIGHT JOIN", SelectOrderBy)
DEFINE_JOIN_SELECT(SelectJoin, SelectFrom, right_join, "RIGHT JOIN", SelectLimit)

DEFINE_JOIN(SelectJoin, SelectFrom, inner_join, "INNER JOIN")
DEFINE_JOIN_SELECT(SelectJoin, SelectFrom, inner_join, "INNER JOIN", SelectFrom)
DEFINE_JOIN_SELECT(SelectJoin, SelectFrom, inner_join, "INNER JOIN", SelectJoinOn)
DEFINE_JOIN_SELECT(SelectJoin, SelectFrom, inner_join, "INNER JOIN", SelectWhere)
DEFINE_JOIN_SELECT(SelectJoin, SelectFrom, inner_join, "INNER JOIN", SelectGroupBy)
DEFINE_JOIN_SELECT(SelectJoin, SelectFrom, inner_join, "INNER JOIN", SelectOrderBy)
DEFINE_JOIN_SELECT(SelectJoin, SelectFrom, inner_join, "INNER JOIN", SelectLimit)

#define DEFINE_WHERE(ResultClass, Class)                                                   \
    template <class... Params>                                                             \
    template <class... WhereParams> /* NOLINTNEXTLINE(bugprone-macro-parentheses) */       \
    ResultClass<Params..., WhereParams&&...> Class<Params...>::where(                      \
        const std::string_view& sql_str, WhereParams&&... where_params                     \
    )&& {                                                                                  \
        throw_assert(meta::count(sql_str, '?') == sizeof...(WhereParams));                 \
        sql += " WHERE ";                                                                  \
        sql += sql_str;                                                                    \
        /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                                   \
        return ResultClass<Params..., WhereParams&&...>{                                   \
            std::move(sql),                                                                \
            std::tuple_cat(                                                                \
                std::move(params),                                                         \
                std::tuple<WhereParams&&...>{std::forward<WhereParams>(where_params)...}   \
            )                                                                              \
        };                                                                                 \
    }                                                                                      \
                                                                                           \
    template <class... Params>                                                             \
    template <class... CondParams> /* NOLINTNEXTLINE(bugprone-macro-parentheses) */        \
    ResultClass<Params..., CondParams...> Class<Params...>::where(                         \
        Condition<CondParams...>&& condition                                               \
    )&& {                                                                                  \
        sql += " WHERE ";                                                                  \
        sql += std::move(condition).sql;                                                   \
        /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                                   \
        return ResultClass<Params..., CondParams...>{                                      \
            std::move(sql), std::tuple_cat(std::move(params), std::move(condition).params) \
        };                                                                                 \
    }

DEFINE_WHERE(SelectWhere, SelectFrom)

#define DEFINE_GROUP_BY(ResultClass, Class)                                                \
    template <class... Params> /* NOLINTNEXTLINE(bugprone-macro-parentheses) */            \
    ResultClass<Params...> Class<Params...>::group_by(const std::string_view& sql_str)&& { \
        throw_assert(meta::count(sql_str, '?') == 0);                                      \
        sql += " GROUP BY ";                                                               \
        sql += sql_str;                                                                    \
        /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                                   \
        return ResultClass<Params...>{std::move(sql), std::move(params)};                  \
    }

DEFINE_GROUP_BY(SelectGroupBy, SelectFrom)

#define DEFINE_ORDER_BY(ResultClass, Class)                                                \
    template <class... Params> /* NOLINTNEXTLINE(bugprone-macro-parentheses) */            \
    ResultClass<Params...> Class<Params...>::order_by(const std::string_view& sql_str)&& { \
        throw_assert(meta::count(sql_str, '?') == 0);                                      \
        sql += " ORDER BY ";                                                               \
        sql += sql_str;                                                                    \
        /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                                   \
        return ResultClass<Params...>{std::move(sql), std::move(params)};                  \
    }

DEFINE_ORDER_BY(SelectOrderBy, SelectFrom)

#define DEFINE_LIMIT(ResultClass, Class)                                                 \
    template <class... Params>                                                           \
    template <class... LimitParams> /* NOLINTNEXTLINE(bugprone-macro-parentheses) */     \
    ResultClass<Params..., LimitParams&&...> Class<Params...>::limit(                    \
        const std::string_view& sql_str, LimitParams&&... limit_params                   \
    )&& {                                                                                \
        throw_assert(meta::count(sql_str, '?') == sizeof...(LimitParams));               \
        sql += " LIMIT ";                                                                \
        sql += sql_str;                                                                  \
        /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                                 \
        return ResultClass<Params..., LimitParams&&...>{                                 \
            std::move(sql),                                                              \
            std::tuple_cat(                                                              \
                std::move(params),                                                       \
                std::tuple<LimitParams&&...>{std::forward<LimitParams>(limit_params)...} \
            )                                                                            \
        };                                                                               \
    }

DEFINE_LIMIT(SelectLimit, SelectFrom)

#define DEFINE_CONVERSION_TO_SQL_WITH_PARAMS(Class)                             \
    template <class... Params> /* NOLINTNEXTLINE(bugprone-macro-parentheses) */ \
    Class<Params...>::operator SqlWithParams<Params...>()&& noexcept {          \
        return SqlWithParams<Params...>{std::move(sql), std::move(params)};     \
    }                                                                           \
                                                                                \
    template <class... Params> /* NOLINTNEXTLINE(bugprone-macro-parentheses) */ \
    SqlWithParams(Class<Params...>&&) -> SqlWithParams<Params...>;

DEFINE_CONVERSION_TO_SQL_WITH_PARAMS(SelectFrom)

// SelectJoin

#define DEFINE_ON(ResultClass, Class)                                                              \
    template <class... Params>                                                                     \
    template <class... OnParams> /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                  \
    ResultClass<Params..., OnParams&&...> Class<Params...>::on(                                    \
        const std::string_view& sql_str, OnParams&&... on_params                                   \
    )&& {                                                                                          \
        throw_assert(meta::count(sql_str, '?') == sizeof...(OnParams));                            \
        sql += " ON ";                                                                             \
        sql += sql_str;                                                                            \
        /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                                           \
        return ResultClass<Params..., OnParams&&...>{                                              \
            std::move(sql),                                                                        \
            std::tuple_cat(                                                                        \
                std::move(params), std::tuple<OnParams&&...>{std::forward<OnParams>(on_params)...} \
            )                                                                                      \
        };                                                                                         \
    }                                                                                              \
                                                                                                   \
    template <class... Params>                                                                     \
    template <class... CondParams> /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                \
    ResultClass<Params..., CondParams...> Class<Params...>::on(                                    \
        Condition<CondParams...>&& condition                                                       \
    )&& {                                                                                          \
        sql += " ON ";                                                                             \
        sql += std::move(condition).sql;                                                           \
        /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                                           \
        return ResultClass<Params..., CondParams...>{                                              \
            std::move(sql), std::tuple_cat(std::move(params), std::move(condition).params)         \
        };                                                                                         \
    }

DEFINE_ON(SelectJoinOn, SelectJoin)

// SelectJoinOn

DEFINE_JOIN(SelectJoin, SelectJoinOn, left_join, "LEFT JOIN")
DEFINE_JOIN_SELECT(SelectJoin, SelectJoinOn, left_join, "LEFT JOIN", SelectFrom)
DEFINE_JOIN_SELECT(SelectJoin, SelectJoinOn, left_join, "LEFT JOIN", SelectJoinOn)
DEFINE_JOIN_SELECT(SelectJoin, SelectJoinOn, left_join, "LEFT JOIN", SelectWhere)
DEFINE_JOIN_SELECT(SelectJoin, SelectJoinOn, left_join, "LEFT JOIN", SelectGroupBy)
DEFINE_JOIN_SELECT(SelectJoin, SelectJoinOn, left_join, "LEFT JOIN", SelectOrderBy)
DEFINE_JOIN_SELECT(SelectJoin, SelectJoinOn, left_join, "LEFT JOIN", SelectLimit)

DEFINE_JOIN(SelectJoin, SelectJoinOn, right_join, "RIGHT JOIN")
DEFINE_JOIN_SELECT(SelectJoin, SelectJoinOn, right_join, "RIGHT JOIN", SelectFrom)
DEFINE_JOIN_SELECT(SelectJoin, SelectJoinOn, right_join, "RIGHT JOIN", SelectJoinOn)
DEFINE_JOIN_SELECT(SelectJoin, SelectJoinOn, right_join, "RIGHT JOIN", SelectWhere)
DEFINE_JOIN_SELECT(SelectJoin, SelectJoinOn, right_join, "RIGHT JOIN", SelectGroupBy)
DEFINE_JOIN_SELECT(SelectJoin, SelectJoinOn, right_join, "RIGHT JOIN", SelectOrderBy)
DEFINE_JOIN_SELECT(SelectJoin, SelectJoinOn, right_join, "RIGHT JOIN", SelectLimit)

DEFINE_JOIN(SelectJoin, SelectJoinOn, inner_join, "INNER JOIN")
DEFINE_JOIN_SELECT(SelectJoin, SelectJoinOn, inner_join, "INNER JOIN", SelectFrom)
DEFINE_JOIN_SELECT(SelectJoin, SelectJoinOn, inner_join, "INNER JOIN", SelectJoinOn)
DEFINE_JOIN_SELECT(SelectJoin, SelectJoinOn, inner_join, "INNER JOIN", SelectWhere)
DEFINE_JOIN_SELECT(SelectJoin, SelectJoinOn, inner_join, "INNER JOIN", SelectGroupBy)
DEFINE_JOIN_SELECT(SelectJoin, SelectJoinOn, inner_join, "INNER JOIN", SelectOrderBy)
DEFINE_JOIN_SELECT(SelectJoin, SelectJoinOn, inner_join, "INNER JOIN", SelectLimit)

DEFINE_WHERE(SelectWhere, SelectJoinOn)

DEFINE_GROUP_BY(SelectGroupBy, SelectJoinOn)

DEFINE_ORDER_BY(SelectOrderBy, SelectJoinOn)

DEFINE_LIMIT(SelectLimit, SelectJoinOn)

DEFINE_CONVERSION_TO_SQL_WITH_PARAMS(SelectJoinOn)

// SelectWhere

DEFINE_GROUP_BY(SelectGroupBy, SelectWhere)

DEFINE_ORDER_BY(SelectOrderBy, SelectWhere)

DEFINE_LIMIT(SelectLimit, SelectWhere)

DEFINE_CONVERSION_TO_SQL_WITH_PARAMS(SelectWhere)

// SelectGroupBy

DEFINE_ORDER_BY(SelectOrderBy, SelectGroupBy)

DEFINE_LIMIT(SelectLimit, SelectGroupBy)

DEFINE_CONVERSION_TO_SQL_WITH_PARAMS(SelectGroupBy)

// SelectOrderBy

DEFINE_LIMIT(SelectLimit, SelectOrderBy)

DEFINE_CONVERSION_TO_SQL_WITH_PARAMS(SelectOrderBy)

// SelectLimit

DEFINE_CONVERSION_TO_SQL_WITH_PARAMS(SelectLimit)

// Condition

template <class... Params>
Condition<Params...>::Condition(std::string&& sql_, Params&&... params_)
: sql{std::move(sql_)}
, params{std::forward<Params>(params_)...} {
    throw_assert(meta::count(sql, '?') == sizeof...(Params));
}

template <class... Params1, class... Params2>
Condition<Params1..., Params2...>
operator&&(Condition<Params1...>&& cond1, Condition<Params2...>&& cond2) {
    auto sql = std::move(cond1).sql;
    sql += " AND ";
    sql += std::move(cond2).sql;
    return Condition<Params1..., Params2...>{
        std::move(sql), std::tuple_cat(std::move(cond1).params, std::move(cond2).params)
    };
}

template <class... Params1, class... Params2>
Condition<Params1..., Params2...>
operator||(Condition<Params1...>&& cond1, Condition<Params2...>&& cond2) {
    auto sql = std::move(cond1).sql;
    sql.insert(sql.begin(), '(');
    sql += " OR ";
    sql += std::move(cond2).sql;
    sql += ")";
    return Condition<Params1..., Params2...>{
        std::move(sql), std::tuple_cat(std::move(cond1).params, std::move(cond2).params)
    };
}

template <class... Params1, class... Params2>
std::optional<Condition<Params1..., Params2...>> operator&&(
    std::optional<Condition<Params1...>>&& cond1, std::optional<Condition<Params2...>>&& cond2
) {
    if (!cond1) {
        return std::move(cond2);
    }
    if (!cond2) {
        return std::move(cond1);
    }
    return std::optional{std::move(*cond1) && std::move(*cond2)};
}

template <class... Params1, class... Params2>
std::optional<Condition<Params1..., Params2...>> operator||(
    std::optional<Condition<Params1...>>&& cond1, std::optional<Condition<Params2...>>&& cond2
) {
    if (!cond1) {
        return std::move(cond2);
    }
    if (!cond2) {
        return std::move(cond1);
    }
    return std::optional{std::move(*cond1) || std::move(*cond2)};
}

// Update

inline Update::Update(std::string&& sql_) : sql{"UPDATE " + std::move(sql_)} {
    throw_assert(meta::count(sql, '?') == 0);
}

template <class... SetParams>
UpdateSet<SetParams&&...>
Update::set(const std::string_view& sql_str, SetParams&&... set_params) && {
    throw_assert(meta::count(sql_str, '?') == sizeof...(SetParams));
    sql += " SET ";
    sql += sql_str;
    return UpdateSet<SetParams&&...>{
        std::move(sql), std::tuple<SetParams&&...>(std::forward<SetParams>(set_params)...)
    };
}

#define DEFINE_NON_TEMPLATE_CLASS_JOIN(ResultClass, Class, join_name, join_str) \
    /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                            \
    inline ResultClass<> Class::join_name(const std::string_view& sql_str)&& {  \
        throw_assert(meta::count(sql_str, '?') == 0);                           \
        sql += " " join_str " ";                                                \
        sql += sql_str;                                                         \
        /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                        \
        return ResultClass<>{std::move(sql), std::tuple{}};                     \
    }

#define DEFINE_NON_TEMPLATE_CLASS_JOIN_SELECT(ResultClass, Class, join_name, join_str, SelectType) \
    template <class... SelectParams> /* NOLINTNEXTLINE(bugprone-macro-parentheses) */              \
    ResultClass<SelectParams...> Class::join_name(                                                 \
        SelectType<SelectParams...>&& select, /* NOLINT(bugprone-macro-parentheses) */             \
        std::string_view table_name                                                                \
    )&& {                                                                                          \
        throw_assert(meta::count(table_name, '?') == 0);                                           \
        sql += " " join_str " (";                                                                  \
        sql += std::move(select).sql;                                                              \
        sql += ") ";                                                                               \
        sql += table_name;                                                                         \
        /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                                           \
        return ResultClass<SelectParams...>{std::move(sql), std::move(select).params};             \
    }

DEFINE_NON_TEMPLATE_CLASS_JOIN(UpdateJoin, Update, left_join, "LEFT JOIN")
DEFINE_NON_TEMPLATE_CLASS_JOIN_SELECT(UpdateJoin, Update, left_join, "LEFT JOIN", SelectFrom)
DEFINE_NON_TEMPLATE_CLASS_JOIN_SELECT(UpdateJoin, Update, left_join, "LEFT JOIN", SelectJoinOn)
DEFINE_NON_TEMPLATE_CLASS_JOIN_SELECT(UpdateJoin, Update, left_join, "LEFT JOIN", SelectWhere)
DEFINE_NON_TEMPLATE_CLASS_JOIN_SELECT(UpdateJoin, Update, left_join, "LEFT JOIN", SelectGroupBy)
DEFINE_NON_TEMPLATE_CLASS_JOIN_SELECT(UpdateJoin, Update, left_join, "LEFT JOIN", SelectOrderBy)
DEFINE_NON_TEMPLATE_CLASS_JOIN_SELECT(UpdateJoin, Update, left_join, "LEFT JOIN", SelectLimit)

DEFINE_NON_TEMPLATE_CLASS_JOIN(UpdateJoin, Update, right_join, "RIGHT JOIN")
DEFINE_NON_TEMPLATE_CLASS_JOIN_SELECT(UpdateJoin, Update, right_join, "RIGHT JOIN", SelectFrom)
DEFINE_NON_TEMPLATE_CLASS_JOIN_SELECT(UpdateJoin, Update, right_join, "RIGHT JOIN", SelectJoinOn)
DEFINE_NON_TEMPLATE_CLASS_JOIN_SELECT(UpdateJoin, Update, right_join, "RIGHT JOIN", SelectWhere)
DEFINE_NON_TEMPLATE_CLASS_JOIN_SELECT(UpdateJoin, Update, right_join, "RIGHT JOIN", SelectGroupBy)
DEFINE_NON_TEMPLATE_CLASS_JOIN_SELECT(UpdateJoin, Update, right_join, "RIGHT JOIN", SelectOrderBy)
DEFINE_NON_TEMPLATE_CLASS_JOIN_SELECT(UpdateJoin, Update, right_join, "RIGHT JOIN", SelectLimit)

DEFINE_NON_TEMPLATE_CLASS_JOIN(UpdateJoin, Update, inner_join, "INNER JOIN")
DEFINE_NON_TEMPLATE_CLASS_JOIN_SELECT(UpdateJoin, Update, inner_join, "INNER JOIN", SelectFrom)
DEFINE_NON_TEMPLATE_CLASS_JOIN_SELECT(UpdateJoin, Update, inner_join, "INNER JOIN", SelectJoinOn)
DEFINE_NON_TEMPLATE_CLASS_JOIN_SELECT(UpdateJoin, Update, inner_join, "INNER JOIN", SelectWhere)
DEFINE_NON_TEMPLATE_CLASS_JOIN_SELECT(UpdateJoin, Update, inner_join, "INNER JOIN", SelectGroupBy)
DEFINE_NON_TEMPLATE_CLASS_JOIN_SELECT(UpdateJoin, Update, inner_join, "INNER JOIN", SelectOrderBy)
DEFINE_NON_TEMPLATE_CLASS_JOIN_SELECT(UpdateJoin, Update, inner_join, "INNER JOIN", SelectLimit)

// UpdateJoin

DEFINE_ON(UpdateJoinOn, UpdateJoin)

// UpdateJoinOn

DEFINE_JOIN(UpdateJoin, UpdateJoinOn, left_join, "LEFT JOIN")
DEFINE_JOIN_SELECT(UpdateJoin, UpdateJoinOn, left_join, "LEFT JOIN", SelectFrom)
DEFINE_JOIN_SELECT(UpdateJoin, UpdateJoinOn, left_join, "LEFT JOIN", SelectJoinOn)
DEFINE_JOIN_SELECT(UpdateJoin, UpdateJoinOn, left_join, "LEFT JOIN", SelectWhere)
DEFINE_JOIN_SELECT(UpdateJoin, UpdateJoinOn, left_join, "LEFT JOIN", SelectGroupBy)
DEFINE_JOIN_SELECT(UpdateJoin, UpdateJoinOn, left_join, "LEFT JOIN", SelectOrderBy)
DEFINE_JOIN_SELECT(UpdateJoin, UpdateJoinOn, left_join, "LEFT JOIN", SelectLimit)

DEFINE_JOIN(UpdateJoin, UpdateJoinOn, right_join, "RIGHT JOIN")
DEFINE_JOIN_SELECT(UpdateJoin, UpdateJoinOn, right_join, "RIGHT JOIN", SelectFrom)
DEFINE_JOIN_SELECT(UpdateJoin, UpdateJoinOn, right_join, "RIGHT JOIN", SelectJoinOn)
DEFINE_JOIN_SELECT(UpdateJoin, UpdateJoinOn, right_join, "RIGHT JOIN", SelectWhere)
DEFINE_JOIN_SELECT(UpdateJoin, UpdateJoinOn, right_join, "RIGHT JOIN", SelectGroupBy)
DEFINE_JOIN_SELECT(UpdateJoin, UpdateJoinOn, right_join, "RIGHT JOIN", SelectOrderBy)
DEFINE_JOIN_SELECT(UpdateJoin, UpdateJoinOn, right_join, "RIGHT JOIN", SelectLimit)

DEFINE_JOIN(UpdateJoin, UpdateJoinOn, inner_join, "INNER JOIN")
DEFINE_JOIN_SELECT(UpdateJoin, UpdateJoinOn, inner_join, "INNER JOIN", SelectFrom)
DEFINE_JOIN_SELECT(UpdateJoin, UpdateJoinOn, inner_join, "INNER JOIN", SelectJoinOn)
DEFINE_JOIN_SELECT(UpdateJoin, UpdateJoinOn, inner_join, "INNER JOIN", SelectWhere)
DEFINE_JOIN_SELECT(UpdateJoin, UpdateJoinOn, inner_join, "INNER JOIN", SelectGroupBy)
DEFINE_JOIN_SELECT(UpdateJoin, UpdateJoinOn, inner_join, "INNER JOIN", SelectOrderBy)
DEFINE_JOIN_SELECT(UpdateJoin, UpdateJoinOn, inner_join, "INNER JOIN", SelectLimit)

template <class... Params>
template <class... SetParams>
UpdateSet<Params..., SetParams&&...>
UpdateJoinOn<Params...>::set(const std::string_view& sql_str, SetParams&&... set_params) && {
    throw_assert(meta::count(sql_str, '?') == sizeof...(SetParams));
    sql += " SET ";
    sql += sql_str;
    return UpdateSet<Params..., SetParams&&...>{
        std::move(sql),
        std::tuple_cat(
            std::move(params), std::tuple<SetParams&&...>{std::forward<SetParams>(set_params)...}
        )
    };
}

// UpdateSet

DEFINE_WHERE(UpdateWhere, UpdateSet)

DEFINE_CONVERSION_TO_SQL_WITH_PARAMS(UpdateSet)

// UpdateWhere

DEFINE_CONVERSION_TO_SQL_WITH_PARAMS(UpdateWhere)

// DeleteFrom

inline DeleteFrom::DeleteFrom(std::string&& sql_) : sql{"DELETE FROM " + std::move(sql_)} {
    throw_assert(meta::count(sql, '?') == 0);
}

template <class... WhereParams>
DeleteWhere<WhereParams&&...>
DeleteFrom::where(const std::string_view& sql_str, WhereParams&&... where_params) && {
    throw_assert(meta::count(sql_str, '?') == sizeof...(WhereParams));
    sql += " WHERE ";
    sql += sql_str;
    return DeleteWhere<WhereParams&&...>{
        std::move(sql), std::tuple<WhereParams&&...>{std::forward<WhereParams>(where_params)...}
    };
}

template <class... CondParams>
DeleteWhere<CondParams...> DeleteFrom::where(Condition<CondParams...>&& condition) && {
    sql += " WHERE ";
    sql += std::move(condition).sql;
    return DeleteWhere<CondParams...>{std::move(sql), std::move(condition).params};
}

inline DeleteFrom::operator SqlWithParams<>() && noexcept {
    return SqlWithParams<>{std::move(sql), std::tuple{}};
}

SqlWithParams(DeleteFrom&&) -> SqlWithParams<>;

// DeleteWhere

DEFINE_CONVERSION_TO_SQL_WITH_PARAMS(DeleteWhere)

// InsertInto

inline InsertInto::InsertInto(std::string&& sql_) : sql{"INSERT INTO " + std::move(sql_)} {
    throw_assert(meta::count(sql, '?') == 0);
}

#define DEFINE_VALUES(ResultClass, Class)                                               \
    template <class... ValuesParams> /* NOLINTNEXTLINE(bugprone-macro-parentheses) */   \
    ResultClass<ValuesParams&&...> Class::values(                                       \
        const std::string_view& sql_str, ValuesParams&&... values_params                \
    )&& {                                                                               \
        throw_assert(meta::count(sql_str, '?') == sizeof...(ValuesParams));             \
        sql += " VALUES(";                                                              \
        sql += sql_str;                                                                 \
        sql += ')';                                                                     \
        /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                                \
        return ResultClass<ValuesParams&&...>{                                          \
            std::move(sql),                                                             \
            std::tuple<ValuesParams&&...>{std::forward<ValuesParams>(values_params)...} \
        };                                                                              \
    }

DEFINE_VALUES(InsertValues, InsertInto)

#define DEFINE_SELECT(ResultClass, Class)                                               \
    template <class... SelectParams> /* NOLINTNEXTLINE(bugprone-macro-parentheses) */   \
    ResultClass<SelectParams&&...> Class::select(                                       \
        const std::string_view& sql_str, SelectParams&&... select_params                \
    )&& {                                                                               \
        throw_assert(meta::count(sql_str, '?') == sizeof...(SelectParams));             \
        sql += " SELECT ";                                                              \
        sql += sql_str;                                                                 \
        /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                                \
        return ResultClass<SelectParams&&...>{                                          \
            std::move(sql),                                                             \
            std::tuple<SelectParams&&...>{std::forward<SelectParams>(select_params)...} \
        };                                                                              \
    }

DEFINE_SELECT(InsertSelect, InsertInto)

// InsertIgnoreInto

inline InsertIgnoreInto::InsertIgnoreInto(std::string&& sql_)
: sql{"INSERT IGNORE INTO " + std::move(sql_)} {
    throw_assert(meta::count(sql, '?') == 0);
}

DEFINE_VALUES(InsertValues, InsertIgnoreInto)

DEFINE_SELECT(InsertSelect, InsertIgnoreInto)

// InsertValues

DEFINE_CONVERSION_TO_SQL_WITH_PARAMS(InsertValues)

// InsertSelect

DEFINE_FROM(InsertSelectFrom, InsertSelect)

// InsertSelectFrom

DEFINE_WHERE(InsertSelectWhere, InsertSelectFrom)

DEFINE_GROUP_BY(InsertSelectGroupBy, InsertSelectFrom)

DEFINE_ORDER_BY(InsertSelectOrderBy, InsertSelectFrom)

DEFINE_CONVERSION_TO_SQL_WITH_PARAMS(InsertSelectFrom)

// InsertSelectWhere

DEFINE_GROUP_BY(InsertSelectGroupBy, InsertSelectWhere)

DEFINE_ORDER_BY(InsertSelectOrderBy, InsertSelectWhere)

DEFINE_CONVERSION_TO_SQL_WITH_PARAMS(InsertSelectWhere)

// InsertSelectGroupBy

DEFINE_ORDER_BY(InsertSelectOrderBy, InsertSelectGroupBy)

DEFINE_CONVERSION_TO_SQL_WITH_PARAMS(InsertSelectGroupBy)

// InsertSelectOrderBy

DEFINE_CONVERSION_TO_SQL_WITH_PARAMS(InsertSelectOrderBy)

#undef DEFINE_FROM
#undef DEFINE_JOIN
#undef DEFINE_JOIN_SELECT
#undef DEFINE_ON
#undef DEFINE_WHERE
#undef DEFINE_GROUP_BY
#undef DEFINE_ORDER_BY
#undef DEFINE_LIMIT
#undef DEFINE_CONVERSION_TO_SQL_WITH_PARAMS
#undef DEFINE_NON_TEMPLATE_CLASS_JOIN
#undef DEFINE_NON_TEMPLATE_CLASS_JOIN_SELECT
#undef DEFINE_VALUES
#undef DEFINE_SELECT

} // namespace sim::sql

#pragma once

#include <simlib/constructor_traits.hh>
#include <type_traits>
#include <utility>

namespace sim::old_sql_fields {

// Warning: this wrapper does not validate against predicate
template <class SqlField, bool (*predicate_func)(const SqlField&), const char* description_str>
class SatisfyingPredicate : public SqlField {
public:
    using underlying_type = SqlField;
    static constexpr auto predicate = predicate_func;
    static constexpr const char* description = description_str;

    SatisfyingPredicate() = default;

    template <
        class... Args,
        std::enable_if_t<
            is_inheriting_implicit_constructor_ok<SatisfyingPredicate, SqlField, Args&&...>,
            int> = 0>
    // NOLINTNEXTLINE(google-explicit-constructor)
    SatisfyingPredicate(Args&&... args) : SqlField{std::forward<Args>(args)...} {}

    template <
        class... Args,
        std::enable_if_t<
            is_inheriting_explicit_constructor_ok<SatisfyingPredicate, SqlField, Args&&...>,
            int> = 0>
    explicit SatisfyingPredicate(Args&&... args) : SqlField{std::forward<Args>(args)...} {}

    SatisfyingPredicate(const SatisfyingPredicate&) = default;
    SatisfyingPredicate(SatisfyingPredicate&&) noexcept = default;
    SatisfyingPredicate& operator=(const SatisfyingPredicate&) = default;
    SatisfyingPredicate& operator=(SatisfyingPredicate&&) noexcept = default;
    ~SatisfyingPredicate() = default;

    using SqlField::operator=;
};

template <class...>
constexpr inline bool is_satisfying_predicate = false;
template <class SqlField, bool (*predicate_func)(const SqlField&), const char* description_str>
constexpr inline bool
    is_satisfying_predicate<SatisfyingPredicate<SqlField, predicate_func, description_str>> = true;

} // namespace sim::old_sql_fields

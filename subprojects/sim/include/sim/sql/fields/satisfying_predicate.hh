#pragma once

#include <simlib/constructor_traits.hh>
#include <simlib/macros/throw.hh>
#include <type_traits>
#include <utility>

namespace sim::sql::fields {

// Warning: this wrapper does not validate against predicate
template <class SqlField, bool (*predicate_func)(const SqlField&), const char* description_str>
class SatisfyingPredicate : public SqlField {
public:
    static constexpr auto predicate = predicate_func;
    static constexpr const char* description = description_str;

    SatisfyingPredicate() = default;

    // NOLINTNEXTLINE(google-explicit-constructor)
    SatisfyingPredicate(SqlField value) : SqlField{std::move(value)} {}

    SatisfyingPredicate(const SatisfyingPredicate&) = default;
    SatisfyingPredicate(SatisfyingPredicate&&) = default;
    SatisfyingPredicate& operator=(const SatisfyingPredicate&) = default;
    SatisfyingPredicate& operator=(SatisfyingPredicate&&) = default;
    ~SatisfyingPredicate() = default;

    SatisfyingPredicate& operator=(SqlField other) {
        SqlField::operator=(static_cast<SqlField&&>(std::move(other)));
        return *this;
    }
};

template <class...>
constexpr inline bool is_satisfying_predicate = false;
template <class SqlField, bool (*predicate_func)(const SqlField&), const char* description_str>
constexpr inline bool
    is_satisfying_predicate<SatisfyingPredicate<SqlField, predicate_func, description_str>> = true;

} // namespace sim::sql::fields

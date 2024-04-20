#pragma once

// TODO: delete this file when it becomes unneeded

#include <cstdint>
#include <sim/old_sql_fields/datetime.hh>
#include <sim/old_sql_fields/satisfying_predicate.hh>
#include <sim/old_sql_fields/varbinary.hh>
#include <sim/primary_key.hh>
#include <simlib/enum_val.hh>
#include <simlib/macros/enum_with_string_conversions.hh>
#include <simlib/string_traits.hh>

namespace sim::users {

struct OldUser {
    ENUM_WITH_STRING_CONVERSIONS(Type, uint8_t,
        (ADMIN, 0, "admin")
        (TEACHER, 1, "teacher")
        (NORMAL, 2, "normal")
    );

    template <class T>
    static bool is_username(const T& username) {
        return std::all_of(username.begin(), username.end(), [](int c) {
            return (is_alnum(c) || c == '_' || c == '-');
        });
    }

    static inline const char is_username_description[] =
        "a string consisting only of the characters [a-zA-Z0-9_-]";

    uint64_t id;
    old_sql_fields::Datetime created_at;
    EnumVal<Type> type;
    old_sql_fields::
        SatisfyingPredicate<old_sql_fields::Varbinary<30>, is_username, is_username_description>
            username;
    old_sql_fields::Varbinary<60> first_name;
    old_sql_fields::Varbinary<60> last_name;
    old_sql_fields::Varbinary<60> email;
    old_sql_fields::Varbinary<64> password_salt;
    old_sql_fields::Varbinary<128> password_hash;

    static constexpr auto primary_key = PrimaryKey{&OldUser::id};
};

} // namespace sim::users

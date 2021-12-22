#pragma once

#include "sim/is_username.hh"
#include "sim/sql_fields/satisfying_predicate.hh"
#include "sim/sql_fields/varbinary.hh"
#include "simlib/enum_val.hh"
#include "simlib/enum_with_string_conversions.hh"

#include <cstdint>

namespace sim::users {

struct User {
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
    };
    static const inline char is_username_description[] =
        "a string consisting only of the characters [a-zA-Z0-9_-]";

    uint64_t id;
    EnumVal<Type> type;
    sql_fields::SatisfyingPredicate<
        sql_fields::Varbinary<30>, is_username, is_username_description>
        username;
    sql_fields::Varbinary<60> first_name;
    sql_fields::Varbinary<60> last_name;
    sql_fields::Varbinary<60> email;
    sql_fields::Varbinary<64> password_salt;
    sql_fields::Varbinary<128> password_hash;
};

constexpr decltype(User::id) SIM_ROOT_UID = 1;

std::pair<decltype(User::password_salt), decltype(User::password_hash)>
salt_and_hash_password(StringView password);

bool password_matches(
    StringView password, StringView password_salt, StringView password_hash) noexcept;

} // namespace sim::users

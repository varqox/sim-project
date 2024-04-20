#pragma once

#include <cstdint>
#include <sim/sql/fields/binary.hh>
#include <sim/sql/fields/datetime.hh>
#include <sim/sql/fields/satisfying_predicate.hh>
#include <sim/sql/fields/varbinary.hh>
#include <simlib/macros/enum_with_string_conversions.hh>
#include <simlib/string_traits.hh>

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
    }

    static constexpr char is_username_description[] =
        "a string consisting only of the characters [a-zA-Z0-9_-]";

    uint64_t id;
    sql::fields::Datetime created_at;
    Type type;
    sql::fields::
        SatisfyingPredicate<sql::fields::Varbinary<30>, is_username, is_username_description>
            username;
    sql::fields::Varbinary<60> first_name;
    sql::fields::Varbinary<60> last_name;
    sql::fields::Varbinary<60> email;
    sql::fields::Binary<64> password_salt; // stored in hex
    sql::fields::Binary<128> password_hash; // stored in hex
};

constexpr decltype(User::id) SIM_ROOT_ID = 1;

std::pair<decltype(User::password_salt), decltype(User::password_hash)>
salt_and_hash_password(std::string_view password);

bool password_matches(
    std::string_view password, std::string_view password_salt, std::string_view password_hash
) noexcept;

} // namespace sim::users

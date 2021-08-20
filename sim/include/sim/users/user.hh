#pragma once

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

    uint64_t id;
    sql_fields::Varbinary<30> username;
    sql_fields::Varbinary<60> first_name;
    sql_fields::Varbinary<60> last_name;
    sql_fields::Varbinary<60> email;
    sql_fields::Varbinary<64> password_salt;
    sql_fields::Varbinary<128> password_hash;
    EnumVal<Type> type;
};

constexpr decltype(User::id) SIM_ROOT_UID = 1;

} // namespace sim::users

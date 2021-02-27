#pragma once

#include "sim/sql_fields/varchar.hh"
#include "simlib/enum_val.hh"

#include <cstdint>

namespace sim::users {

struct User {
    enum class Type : uint8_t { ADMIN = 0, TEACHER = 1, NORMAL = 2 };

    uint64_t id;
    sql_fields::Varchar<30> username;
    sql_fields::Varchar<60> first_name;
    sql_fields::Varchar<60> last_name;
    sql_fields::Varchar<60> email;
    sql_fields::Varchar<64> salt;
    sql_fields::Varchar<128> password;
    EnumVal<Type> type;
};

constexpr decltype(User::id) SIM_ROOT_UID = 1;

} // namespace sim::users

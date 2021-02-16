#pragma once

#include "sim/varchar_field.hh"
#include "simlib/meta.hh"
#include "simlib/mysql.hh"

#include <cstdint>

namespace sim {

struct User {
    enum class Type : uint8_t { ADMIN = 0, TEACHER = 1, NORMAL = 2 };

    uintmax_t id;
    VarcharField<30> username;
    VarcharField<60> first_name;
    VarcharField<60> last_name;
    VarcharField<60> email;
    VarcharField<64> salt;
    VarcharField<128> password;
    EnumVal<Type> type;
};

constexpr uintmax_t SIM_ROOT_UID = 1;

} // namespace sim

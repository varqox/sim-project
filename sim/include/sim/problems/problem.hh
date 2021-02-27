#pragma once

#include "sim/sql_fields/blob.hh"
#include "sim/sql_fields/datetime.hh"
#include "sim/sql_fields/varchar.hh"
#include "sim/users/user.hh"

#include <cstdint>

namespace sim::problems {

struct Problem {
    enum class Type : uint8_t {
        PUBLIC = 1,
        PRIVATE = 2,
        CONTEST_ONLY = 3,
    };

    uint64_t id;
    uint64_t file_id;
    EnumVal<Type> type;
    sql_fields::Varchar<128> name;
    sql_fields::Varchar<64> label;
    sql_fields::Blob<4096> simfile;
    std::optional<decltype(users::User::id)> owner;
    sql_fields::Datetime added;
    sql_fields::Datetime last_edit;
};

} // namespace sim::problems

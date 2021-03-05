#pragma once

#include "sim/internal_files/internal_file.hh"
#include "sim/sql_fields/blob.hh"
#include "sim/sql_fields/datetime.hh"
#include "sim/sql_fields/varbinary.hh"
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
    decltype(internal_files::InternalFile::id) file_id;
    EnumVal<Type> type;
    sql_fields::Varbinary<128> name;
    sql_fields::Varbinary<64> label;
    sql_fields::Blob<4096> simfile;
    std::optional<decltype(users::User::id)> owner;
    sql_fields::Datetime added;
    sql_fields::Datetime last_edit;

    static constexpr uint64_t new_statement_max_size = 10 << 20; // 10 MiB
};

} // namespace sim::problems

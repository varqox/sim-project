#pragma once

#include <cstdint>
#include <sim/internal_files/internal_file.hh>
#include <sim/primary_key.hh>
#include <sim/sql_fields/blob.hh>
#include <sim/sql_fields/datetime.hh>
#include <sim/sql_fields/varbinary.hh>
#include <sim/users/user.hh>
#include <simlib/enum_with_string_conversions.hh>

namespace sim::problems {

struct Problem {
    ENUM_WITH_STRING_CONVERSIONS(Type, uint8_t,
        (PUBLIC, 1, "public")
        (PRIVATE, 2, "private")
        (CONTEST_ONLY, 3, "contest_only")
    );

    uint64_t id;
    decltype(internal_files::InternalFile::id) file_id;
    EnumVal<Type> type;
    sql_fields::Varbinary<128> name;
    sql_fields::Varbinary<64> label;
    sql_fields::Blob<4096> simfile;
    std::optional<decltype(users::User::id)> owner_id;
    sql_fields::Datetime created_at;
    sql_fields::Datetime updated_at;

    static constexpr auto primary_key = PrimaryKey{&Problem::id};

    static constexpr uint64_t new_statement_max_size = 10 << 20; // 10 MiB
};

} // namespace sim::problems

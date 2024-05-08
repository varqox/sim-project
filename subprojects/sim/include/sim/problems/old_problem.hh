#pragma once

// TODO: delete this file when it becomes unneeded

#include <cstdint>
#include <sim/internal_files/old_internal_file.hh>
#include <sim/old_sql_fields/blob.hh>
#include <sim/old_sql_fields/datetime.hh>
#include <sim/old_sql_fields/varbinary.hh>
#include <sim/primary_key.hh>
#include <sim/users/user.hh>
#include <simlib/enum_val.hh>
#include <simlib/macros/enum_with_string_conversions.hh>

namespace sim::problems {

struct OldProblem {
    ENUM_WITH_STRING_CONVERSIONS(Type, uint8_t,
        (PUBLIC, 1, "public")
        (PRIVATE, 2, "private")
        (CONTEST_ONLY, 3, "contest_only")
    );

    uint64_t id;
    old_sql_fields::Datetime created_at;
    decltype(internal_files::OldInternalFile::id) file_id;
    EnumVal<Type> type;
    old_sql_fields::Varbinary<128> name;
    old_sql_fields::Varbinary<64> label;
    old_sql_fields::Blob<4096> simfile;
    std::optional<decltype(users::User::id)> owner_id;
    old_sql_fields::Datetime updated_at;

    static constexpr auto primary_key = PrimaryKey{&OldProblem::id};

    static constexpr uint64_t new_statement_max_size = 10 << 20; // 10 MiB
};

} // namespace sim::problems

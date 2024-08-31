#pragma once

#include <cstdint>
#include <sim/internal_files/internal_file.hh>
#include <sim/sql/fields/blob.hh>
#include <sim/sql/fields/datetime.hh>
#include <sim/sql/fields/varbinary.hh>
#include <sim/users/user.hh>
#include <simlib/macros/enum_with_string_conversions.hh>

namespace sim::problems {

struct Problem {
    ENUM_WITH_STRING_CONVERSIONS(Visibility, uint8_t,
        (PUBLIC, 1, "public")
        (PRIVATE, 2, "private")
        (CONTEST_ONLY, 3, "contest_only")
    );

    uint64_t id;
    sql::fields::Datetime created_at;
    decltype(internal_files::InternalFile::id) file_id;
    Visibility visibility;
    sql::fields::Varbinary<128> name;
    sql::fields::Varbinary<64> label;
    sql::fields::Blob simfile;
    std::optional<decltype(users::User::id)> owner_id;
    sql::fields::Datetime updated_at;
    static constexpr size_t COLUMNS_NUM = 9;

    static constexpr uint64_t NEW_STATEMENT_MAX_SIZE = 10 << 20; // 10 MiB
};

} // namespace sim::problems

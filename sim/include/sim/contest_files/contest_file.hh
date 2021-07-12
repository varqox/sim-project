#pragma once

#include "sim/contests/contest.hh"
#include "sim/internal_files/internal_file.hh"
#include "sim/sql_fields/datetime.hh"
#include "sim/sql_fields/varbinary.hh"
#include "sim/users/user.hh"

#include <cstdio>

namespace sim::contest_files {

struct ContestFile {
    sql_fields::Varbinary<30> id;
    decltype(internal_files::InternalFile::id) file_id;
    decltype(contests::Contest::id) contest_id;
    sql_fields::Varbinary<128> name;
    sql_fields::Varbinary<512> description;
    uint64_t file_size;
    sql_fields::Datetime modified;
    std::optional<decltype(users::User::id)> creator;

    static constexpr decltype(file_size) max_size = 128 << 20; // 128 MiB [bytes]
};

} // namespace sim::contest_files

#pragma once

#include <cstdint>
#include <sim/contests/contest.hh>
#include <sim/internal_files/internal_file.hh>
#include <sim/sql/fields/binary.hh>
#include <sim/sql/fields/datetime.hh>
#include <sim/sql/fields/varbinary.hh>
#include <sim/users/user.hh>

namespace sim::contest_files {

struct ContestFile {
    sql::fields::Binary<30> id;
    decltype(internal_files::InternalFile::id) file_id;
    decltype(contests::Contest::id) contest_id;
    sql::fields::Varbinary<128> name;
    sql::fields::Varbinary<512> description;
    uint64_t file_size; // in bytes
    sql::fields::Datetime modified;
    std::optional<decltype(users::User::id)> creator;
    static constexpr size_t COLUMNS_NUM = 8;

    static constexpr decltype(file_size) MAX_SIZE = 128 << 20; // 128 MiB [bytes]
};

} // namespace sim::contest_files

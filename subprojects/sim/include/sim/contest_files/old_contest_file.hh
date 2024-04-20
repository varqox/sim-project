#pragma once

// TODO: delete this file when it becomes unneeded

#include <cstdint>
#include <sim/contests/old_contest.hh>
#include <sim/internal_files/old_internal_file.hh>
#include <sim/old_sql_fields/datetime.hh>
#include <sim/old_sql_fields/varbinary.hh>
#include <sim/primary_key.hh>
#include <sim/users/user.hh>

namespace sim::contest_files {

struct OldContestFile {
    old_sql_fields::Varbinary<30> id;
    decltype(internal_files::OldInternalFile::id) file_id;
    decltype(contests::OldContest::id) contest_id;
    old_sql_fields::Varbinary<128> name;
    old_sql_fields::Varbinary<512> description;
    uint64_t file_size;
    old_sql_fields::Datetime modified;
    std::optional<decltype(users::User::id)> creator;

    static constexpr auto primary_key = PrimaryKey{&OldContestFile::id};

    static constexpr decltype(file_size) max_size = 128 << 20; // 128 MiB [bytes]
};

} // namespace sim::contest_files

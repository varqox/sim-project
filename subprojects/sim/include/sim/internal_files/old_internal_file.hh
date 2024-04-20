#pragma once

// TODO: delete this file when it becomes unneeded

#include <cstdint>
#include <sim/old_sql_fields/datetime.hh>
#include <sim/primary_key.hh>
#include <simlib/concat.hh>
#include <simlib/string_view.hh>

namespace sim::internal_files {

struct OldInternalFile {
    uint64_t id;
    old_sql_fields::Datetime created_at;

    static constexpr auto primary_key = PrimaryKey{&OldInternalFile::id};
};

constexpr CStringView dir = "internal_files/";

inline auto path_of(decltype(OldInternalFile::id) id) { return concat<64>(dir, id); }

inline auto path_of(const OldInternalFile& internal_file) { return path_of(internal_file.id); }

} // namespace sim::internal_files

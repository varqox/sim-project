#pragma once

#include <cstdint>
#include <sim/sql/fields/datetime.hh>
#include <simlib/concat_tostr.hh>
#include <string>

namespace sim::internal_files {

struct InternalFile {
    uint64_t id;
    sql::fields::Datetime created_at;
};

inline std::string path_of(decltype(InternalFile::id) id) {
    return concat_tostr("internal_files/", id);
}

} // namespace sim::internal_files

#pragma once

#include "simlib/concat.hh"
#include "simlib/string_view.hh"

#include <cstdint>

namespace sim::internal_files {

struct InternalFile {
    uint64_t id;
};

constexpr CStringView dir = "internal_files/";

inline auto path_of(decltype(InternalFile::id) id) { return concat<64>(dir, id); }

inline auto path_of(const InternalFile& internal_file) { return path_of(internal_file.id); }

} // namespace sim::internal_files

#pragma once

#include <cstdint>
#include <sim/sql/fields/datetime.hh>

namespace sim::internal_files {

struct InternalFile {
    uint64_t id;
    sql::fields::Datetime created_at;
};

} // namespace sim::internal_files

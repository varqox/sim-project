#pragma once

#include "sim/blob_field.hh"
#include "sim/datetime_field.hh"
#include "sim/user.hh"
#include "sim/varchar_field.hh"

namespace sim {

struct Problem {
    enum class Type : uint8_t {
        PUBLIC = 1,
        PRIVATE = 2,
        CONTEST_ONLY = 3,
    };

    uintmax_t id;
    uintmax_t file_id;
    EnumVal<Type> type;
    VarcharField<128> name;
    VarcharField<64> label;
    BlobField<4096> simfile;
    std::optional<decltype(User::id)> owner;
    DatetimeField added;
    DatetimeField last_edit;
};

} // namespace sim

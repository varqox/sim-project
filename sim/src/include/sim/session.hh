#pragma once

#include "sim/blob_field.hh"
#include "sim/datetime_field.hh"
#include "sim/user.hh"
#include "sim/varchar_field.hh"

#include <cstdint>

namespace sim {

struct Session {
    VarcharField<30> id;
    VarcharField<20> csrf_token;
    decltype(sim::User::id) user_id;
    BlobField<32> data;
    VarcharField<15> ip;
    BlobField<128> user_agent;
    DatetimeField expires;
};

} // namespace sim

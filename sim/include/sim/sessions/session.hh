#pragma once

#include "sim/sql_fields/blob.hh"
#include "sim/sql_fields/datetime.hh"
#include "sim/sql_fields/varchar.hh"
#include "sim/users/user.hh"

#include <cstdint>

namespace sim::sessions {

struct Session {
    sql_fields::Varchar<30> id;
    sql_fields::Varchar<20> csrf_token;
    decltype(sim::users::User::id) user_id;
    sql_fields::Blob<32> data;
    sql_fields::Varchar<15> ip;
    sql_fields::Blob<128> user_agent;
    sql_fields::Datetime expires;
};

} // namespace sim::sessions

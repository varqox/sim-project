#pragma once

#include <chrono>
#include <cstdint>
#include <sim/primary_key.hh>
#include <sim/sql_fields/blob.hh>
#include <sim/sql_fields/datetime.hh>
#include <sim/sql_fields/varbinary.hh>
#include <sim/users/user.hh>

namespace sim::sessions {

struct Session {
    sql_fields::Varbinary<30> id;
    sql_fields::Varbinary<20> csrf_token;
    decltype(sim::users::User::id) user_id;
    sql_fields::Blob<32> data;
    sql_fields::Blob<128> user_agent;
    sql_fields::Datetime expires;

    constexpr static auto primary_key = PrimaryKey{&Session::id};

    static constexpr auto short_session_max_lifetime = std::chrono::hours{1};
    static constexpr auto long_session_max_lifetime = std::chrono::hours{30 * 24}; // 30 days
};

} // namespace sim::sessions

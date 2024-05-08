#pragma once

// TODO: delete this file when it becomes unneeded

#include <chrono>
#include <sim/old_sql_fields/blob.hh>
#include <sim/old_sql_fields/datetime.hh>
#include <sim/old_sql_fields/varbinary.hh>
#include <sim/primary_key.hh>
#include <sim/users/old_user.hh>
#include <sim/users/user.hh>

namespace sim::sessions {

struct OldSession {
    old_sql_fields::Varbinary<30> id;
    old_sql_fields::Varbinary<20> csrf_token;
    decltype(sim::users::User::id) user_id;
    old_sql_fields::Blob<32> data;
    old_sql_fields::Blob<128> user_agent;
    old_sql_fields::Datetime expires;

    static constexpr auto primary_key = PrimaryKey{&OldSession::id};

    static constexpr auto short_session_max_lifetime = std::chrono::hours{1};
    static constexpr auto long_session_max_lifetime = std::chrono::hours{30 * 24}; // 30 days
};

} // namespace sim::sessions

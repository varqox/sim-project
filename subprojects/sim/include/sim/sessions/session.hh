#pragma once

#include <chrono>
#include <sim/sql/fields/binary.hh>
#include <sim/sql/fields/blob.hh>
#include <sim/sql/fields/datetime.hh>
#include <sim/users/user.hh>

namespace sim::sessions {

struct Session {
    sql::fields::Binary<30> id;
    sql::fields::Binary<20> csrf_token;
    decltype(users::User::id) user_id;
    sql::fields::Blob data;
    sql::fields::Blob user_agent;
    sql::fields::Datetime expires;

    static constexpr auto SHORT_SESSION_MAX_LIFETIME = std::chrono::hours{1};
    static constexpr auto LONG_SESSION_MAX_LIFETIME = std::chrono::hours{30 * 24};
};

} // namespace sim::sessions

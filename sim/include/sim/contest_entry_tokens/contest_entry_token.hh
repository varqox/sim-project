#pragma once

#include "sim/contests/contest.hh"
#include "sim/sql_fields/datetime.hh"
#include "sim/sql_fields/varchar.hh"

#include <chrono>

namespace sim::contest_entry_tokens {

struct ContestEntryToken {
    sql_fields::Varchar<48> token;
    decltype(contests::Contest::id) contest_id;
    std::optional<sql_fields::Varchar<8>> short_token;
    std::optional<sql_fields::Datetime> short_token_expiration;

    static constexpr auto short_token_max_lifetime = std::chrono::hours{1};
};

} // namespace sim::contest_entry_tokens

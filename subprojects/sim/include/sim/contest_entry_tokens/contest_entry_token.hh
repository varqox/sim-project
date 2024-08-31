#pragma once

#include <chrono>
#include <optional>
#include <sim/contests/contest.hh>
#include <sim/sql/fields/binary.hh>
#include <sim/sql/fields/datetime.hh>

namespace sim::contest_entry_tokens {

struct ContestEntryToken {
    sql::fields::Binary<48> token;
    decltype(contests::Contest::id) contest_id;
    std::optional<sql::fields::Binary<8>> short_token;
    std::optional<sql::fields::Datetime> short_token_expiration;
    static constexpr size_t COLUMNS_NUM = 4;

    static constexpr auto SHORT_TOKEN_MAX_LIFETIME = std::chrono::hours{1};
};

} // namespace sim::contest_entry_tokens

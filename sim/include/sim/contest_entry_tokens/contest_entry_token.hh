#pragma once

#include <chrono>
#include <optional>
#include <sim/contests/contest.hh>
#include <sim/primary_key.hh>
#include <sim/sql_fields/datetime.hh>
#include <sim/sql_fields/varbinary.hh>

namespace sim::contest_entry_tokens {

struct ContestEntryToken {
    sql_fields::Varbinary<48> token;
    decltype(contests::Contest::id) contest_id;
    std::optional<sql_fields::Varbinary<8>> short_token;
    std::optional<sql_fields::Datetime> short_token_expiration;

    static constexpr auto primary_key = PrimaryKey{&ContestEntryToken::token};

    static constexpr auto short_token_max_lifetime = std::chrono::hours{1};
};

} // namespace sim::contest_entry_tokens

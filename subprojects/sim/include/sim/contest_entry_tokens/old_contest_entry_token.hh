#pragma once

// TODO: delete this file when it becomes unneeded

#include <chrono>
#include <optional>
#include <sim/contests/old_contest.hh>
#include <sim/old_sql_fields/datetime.hh>
#include <sim/old_sql_fields/varbinary.hh>
#include <sim/primary_key.hh>

namespace sim::contest_entry_tokens {

struct OldContestEntryToken {
    old_sql_fields::Varbinary<48> token;
    decltype(contests::OldContest::id) contest_id;
    std::optional<old_sql_fields::Varbinary<8>> short_token;
    std::optional<old_sql_fields::Datetime> short_token_expiration;

    static constexpr auto primary_key = PrimaryKey{&OldContestEntryToken::token};

    static constexpr auto short_token_max_lifetime = std::chrono::hours{1};
};

} // namespace sim::contest_entry_tokens

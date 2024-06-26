#pragma once

#include "../web_worker/context.hh"
#include "contest.hh"

#include <optional>
#include <sim/contest_users/contest_user.hh>

namespace web_server::capabilities {

struct ContestEntryToken {
    bool view : 1;
    bool create : 1;
    bool regenerate : 1;
    bool delete_ : 1;
    bool use : 1;
    bool view_contest_name : 1;
};

enum class ContestEntryTokenKind {
    NORMAL,
    SHORT,
};

ContestEntryToken contest_entry_token_for(
    ContestEntryTokenKind token_kind,
    const decltype(web_worker::Context::session)& session,
    const Contest& caps_contest,
    std::optional<decltype(sim::contest_users::ContestUser::mode)> contest_user_mode
) noexcept;

} // namespace web_server::capabilities

#pragma once

#include "sim/contest_users/contest_user.hh"
#include "sim/contests/contest.hh"
#include "simlib/mysql/mysql.hh"
#include "src/web_server/web_worker/context.hh"

#include <optional>

namespace web_server::capabilities {

struct ContestNode {
    bool view : 1;
    bool edit : 1;
    bool delete_ : 1;
    // submissions
    bool view_my_submissions : 1;
    bool view_all_submissions : 1;
    bool view_final_submissions : 1;
    // ranking
    bool view_ranking : 1;
    bool view_fully_revealed_ranking : 1;
};

struct Contest {
    ContestNode node;
    bool make_public : 1;
    bool make_private : 1;
    bool change_name : 1;
    bool view_all_rounds : 1;
    bool add_round : 1;
    bool view_all_problems : 1;

    struct Files {
        bool view : 1;
        bool view_creator : 1;
        bool add : 1;
    } files;

    struct Users {
        bool view : 1;
        bool add_owner : 1;
        bool add_moderator : 1;
        bool add_contestant : 1;
    } users;

    struct EntryTokens {
        bool view : 1;
    } entry_tokens;
};

Contest contest_for(const decltype(web_worker::Context::session)& session,
        decltype(sim::contests::Contest::is_public) contest_is_public,
        std::optional<decltype(sim::contest_users::ContestUser::mode)> contest_user_mode) noexcept;

// Returns std::nullopt if such contest does not exist
std::optional<std::pair<Contest, std::optional<decltype(sim::contest_users::ContestUser::mode)>>>
contest_for(mysql::Connection& mysql, const decltype(web_worker::Context::session)& session,
        decltype(sim::contests::Contest::id) contest_id);

} // namespace web_server::capabilities

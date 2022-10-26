#include "src/web_server/capabilities/contest_entry_token.hh"
#include "sim/contest_users/contest_user.hh"
#include "sim/users/user.hh"
#include "src/web_server/capabilities/contest.hh"
#include "src/web_server/capabilities/utils.hh"
#include "src/web_server/web_worker/context.hh"

#include <cstdlib>

using sim::contest_users::ContestUser;

namespace web_server::capabilities {

ContestEntryToken contest_entry_token_for(ContestEntryTokenKind token_kind,
        const decltype(web_worker::Context::session)& session, const Contest& caps_contest,
        std::optional<decltype(sim::contest_users::ContestUser::mode)> contest_user_mode) noexcept {
    bool is_contest_moderator = caps_contest.node.view and
            (is_admin(session) or
                    is_one_of(contest_user_mode, ContestUser::Mode::OWNER,
                            ContestUser::Mode::MODERATOR));
    switch (token_kind) {
    case ContestEntryTokenKind::NORMAL:
    case ContestEntryTokenKind::SHORT:
        return ContestEntryToken{
                .view = is_contest_moderator,
                .create = is_contest_moderator,
                .regen = is_contest_moderator,
                .delete_ = is_contest_moderator,
                .use = true,
                .view_contest_name = true,
        };
    }
    std::abort();
}

} // namespace web_server::capabilities

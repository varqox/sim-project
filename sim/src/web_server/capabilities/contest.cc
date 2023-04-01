#include "contest.hh"
#include "utils.hh"

#include <cstdint>
#include <sim/contest_users/contest_user.hh>
#include <sim/users/user.hh>
#include <simlib/debug.hh>
#include <simlib/mysql/mysql.hh>
#include <simlib/utilities.hh>

using sim::contest_users::ContestUser;

namespace web_server::capabilities {

Contest contest_for(
    const decltype(web_worker::Context::session)& session,
    decltype(sim::contests::Contest::is_public) contest_is_public,
    std::optional<decltype(ContestUser::mode)> contest_user_mode
) noexcept {
    bool is_contest_moderator = is_admin(session) or
        is_one_of(contest_user_mode, ContestUser::Mode::OWNER, ContestUser::Mode::MODERATOR);
    bool has_access = is_admin(session) or contest_is_public or contest_user_mode;
    return Contest{
        .node =
            {
                .view = has_access,
                .edit = is_contest_moderator,
                .delete_ = is_admin(session) or contest_user_mode == ContestUser::Mode::OWNER,
                .view_my_submissions = has_access and session,
                .view_all_submissions = is_contest_moderator,
                .view_final_submissions = is_contest_moderator,
                .view_ranking = has_access,
                .view_fully_revealed_ranking = is_contest_moderator,
            },
        .make_public = is_admin(session),
        .make_private = is_contest_moderator,
        .change_name = is_contest_moderator,
        .view_all_rounds = is_contest_moderator,
        .add_round = is_contest_moderator,
        .view_all_problems = is_contest_moderator,
        .files =
            {
                .view = has_access,
                .view_creator = is_contest_moderator,
                .add = is_contest_moderator,
            },
        .users{
            .view = is_contest_moderator,
            .add_owner = is_admin(session) or contest_user_mode == ContestUser::Mode::OWNER,
            .add_moderator = is_contest_moderator,
            .add_contestant = is_contest_moderator,
        },
        .entry_tokens =
            {
                .view = is_contest_moderator,
            },
    };
}

std::optional<std::pair<Contest, std::optional<decltype(sim::contest_users::ContestUser::mode)>>>
contest_for(
    mysql::Connection& mysql,
    const decltype(web_worker::Context::session)& session,
    decltype(sim::contests::Contest::id) contest_id
) {
    STACK_UNWINDING_MARK;

    decltype(sim::contests::Contest::is_public) is_public;
    mysql::Optional<decltype(ContestUser::mode)> contest_user_mode;
    mysql::Statement stmt;
    if (session) {
        stmt = mysql.prepare("SELECT c.is_public, cu.mode FROM contests c "
                             "LEFT JOIN contest_users cu ON cu.contest_id=c.id AND cu.user_id=? "
                             "WHERE c.id=?");
        stmt.bind_and_execute(session->user_id, contest_id);
        stmt.res_bind_all(is_public, contest_user_mode);
    } else {
        stmt = mysql.prepare("SELECT is_public FROM contests WHERE id=?");
        stmt.bind_and_execute(contest_id);
        stmt.res_bind_all(is_public);
    }
    if (not stmt.next()) {
        return std::nullopt;
    }
    return std::pair{
        contest_for(session, is_public, contest_user_mode), contest_user_mode.to_opt()};
}

} // namespace web_server::capabilities

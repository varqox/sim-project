#include "api.hh"
#include "../capabilities/contest.hh"
#include "../capabilities/contest_entry_token.hh"
#include "../http/response.hh"
#include "../web_worker/context.hh"
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <sim/contest_entry_tokens/contest_entry_token.hh>
#include <sim/contest_users/contest_user.hh>
#include <sim/contests/contest.hh>
#include <sim/random.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/json_str/json_str.hh>
#include <simlib/mysql/mysql.hh>
#include <simlib/string_transform.hh>
#include <simlib/string_view.hh>
#include <simlib/time.hh>

using sim::contest_entry_tokens::ContestEntryToken;
using sim::contest_users::ContestUser;
using sim::contests::Contest;
using web_server::capabilities::ContestEntryTokenKind;
using web_server::http::Response;
using web_server::web_worker::Context;

namespace {

struct TokensInfo {
    decltype(Contest::is_public) contest_is_public;
    mysql::Optional<decltype(ContestUser::mode)> cu_mode;
    mysql::Optional<decltype(ContestEntryToken::token)> token;
    mysql::Optional<decltype(ContestEntryToken::short_token)::value_type> short_token;
    mysql::Optional<decltype(ContestEntryToken::short_token_expiration)::value_type>
            short_token_expiration;
    std::string curr_time;
    web_server::capabilities::Contest caps_contest;
};

std::optional<TokensInfo> tokens_info_for(Context& ctx, decltype(Contest::id) contest_id) {
    TokensInfo ti;
    auto stmt =
            ctx.mysql.prepare("SELECT c.is_public, cu.mode, cet.token, cet.short_token, "
                              "cet.short_token_expiration "
                              "FROM contests c "
                              "LEFT JOIN contest_users cu ON cu.contest_id=c.id AND cu.user_id=? "
                              "LEFT JOIN contest_entry_tokens cet ON cet.contest_id=c.id "
                              "WHERE c.id=?");
    stmt.bind_and_execute(
            ctx.session ? std::optional{ctx.session->user_id} : std::nullopt, contest_id);
    stmt.res_bind_all(
            ti.contest_is_public, ti.cu_mode, ti.token, ti.short_token, ti.short_token_expiration);
    if (not stmt.next()) {
        return std::nullopt;
    }
    ti.curr_time = mysql_date();
    assert(ti.short_token.has_value() == ti.short_token_expiration.has_value());
    if (ti.short_token and ti.curr_time >= *ti.short_token_expiration) {
        ti.short_token = std::nullopt;
        ti.short_token_expiration = std::nullopt;
    }
    ti.caps_contest =
            web_server::capabilities::contest_for(ctx.session, ti.contest_is_public, ti.cu_mode);
    return ti;
}

template <class Func>
Response with_tokens_info(Context& ctx, decltype(Contest::id) contest_id, Func&& func) {
    auto ti_opt = tokens_info_for(ctx, contest_id);
    if (not ti_opt) {
        return ctx.response_404();
    }
    auto resp = std::forward<Func>(func)(std::move(*ti_opt));
    return resp;
}

struct TokenContestInfo {
    decltype(Contest::id) contest_id;
    decltype(Contest::name) contest_name;
    web_server::capabilities::Contest caps_contest;
    web_server::capabilities::ContestEntryToken caps_token;
};

std::optional<TokenContestInfo> token_contest_info_for(
        Context& ctx, StringView token_or_short_token) {
    TokenContestInfo tci;
    decltype(Contest::is_public) contest_is_public;
    mysql::Optional<decltype(ContestUser::mode)> cu_mode;
    static_assert(decltype(ContestEntryToken::token)::max_len !=
                    decltype(ContestEntryToken::short_token)::value_type::max_len,
            "These cannot be equal because this would cause conflict in selecting the token "
            "in "
            "the below query");
    auto token_kind = token_or_short_token.size() == decltype(ContestEntryToken::token)::max_len
            ? ContestEntryTokenKind::NORMAL
            : ContestEntryTokenKind::SHORT;
    auto stmt = ctx.mysql.prepare(
            "SELECT c.id, c.name, c.is_public, cu.mode "
            "FROM contest_entry_tokens cet "
            "LEFT JOIN contests c ON c.id = cet.contest_id "
            "LEFT JOIN contest_users cu ON cu.contest_id=c.id AND cu.user_id=? "
            "WHERE cet.token=? OR (cet.short_token=? AND cet.short_token_expiration<?)");
    stmt.bind_and_execute(ctx.session ? std::optional{ctx.session->user_id} : std::nullopt,
            token_or_short_token, token_or_short_token, mysql_date());
    stmt.res_bind_all(tci.contest_id, tci.contest_name, contest_is_public, cu_mode);
    if (not stmt.next()) {
        return std::nullopt;
    }
    tci.caps_contest =
            web_server::capabilities::contest_for(ctx.session, contest_is_public, cu_mode);
    tci.caps_token = web_server::capabilities::contest_entry_token_for(
            token_kind, ctx.session, tci.caps_contest, cu_mode);
    return tci;
}

template <class Func>
Response with_token_contest_info(Context& ctx, StringView token_or_short_token, Func&& func) {
    auto tci_opt = token_contest_info_for(ctx, token_or_short_token);
    if (not tci_opt) {
        return ctx.response_404();
    }
    auto resp = std::forward<Func>(func)(std::move(*tci_opt));
    return resp;
}

auto random_token() {
    return sim::generate_random_token(decltype(ContestEntryToken::token)::max_len);
}

auto random_short_token() {
    return sim::generate_random_token(
            decltype(ContestEntryToken::short_token)::value_type::max_len);
}

} // namespace

namespace web_server::contest_entry_tokens::api {

Response view(Context& ctx, decltype(Contest::id) contest_id) {
    return with_tokens_info(ctx, contest_id, [&](TokensInfo&& ti) {
        if (not ti.caps_contest.entry_tokens.view) {
            return ctx.response_403();
        }

        json_str::Object obj;
        auto prop_capabilites = [&](auto& obj, const capabilities::ContestEntryToken& caps) {
            obj.prop_obj("capabilities", [&](auto& obj) {
                obj.prop("create", caps.create);
                obj.prop("regen", caps.regen);
                obj.prop("delete", caps.delete_);
            });
        };

        auto token_caps = capabilities::contest_entry_token_for(
                ContestEntryTokenKind::NORMAL, ctx.session, ti.caps_contest, ti.cu_mode);
        if (token_caps.view) {
            obj.prop_obj("token", [&](auto& obj) {
                obj.prop("value", ti.token);
                prop_capabilites(obj, token_caps);
            });
        }

        auto short_token_caps =
                capabilities::contest_entry_token_for(capabilities::ContestEntryTokenKind::SHORT,
                        ctx.session, ti.caps_contest, ti.cu_mode);
        if (short_token_caps.view) {
            obj.prop_obj("short_token", [&](auto& obj) {
                obj.prop("value", ti.short_token);
                obj.prop("expires_at", ti.short_token_expiration);
                prop_capabilites(obj, short_token_caps);
            });
        }

        return ctx.response_json(std::move(obj).into_str());
    });
}

Response add(Context& ctx, decltype(Contest::id) contest_id) {
    return with_tokens_info(ctx, contest_id, [&](TokensInfo&& ti) {
        auto token_caps = capabilities::contest_entry_token_for(
                ContestEntryTokenKind::NORMAL, ctx.session, ti.caps_contest, ti.cu_mode);
        if (not token_caps.create) {
            return ctx.response_403();
        }
        if (ti.token) {
            return ctx.response_400("Contest already has an entry token");
        }
        auto stmt =
                ctx.mysql.prepare("INSERT IGNORE contest_entry_tokens(token, contest_id, "
                                  "short_token, short_token_expiration) VALUES(?, ?, NULL, NULL)");
        do {
            stmt.bind_and_execute(random_token(), contest_id);
        } while (stmt.affected_rows() == 0);
        return ctx.response_ok();
    });
}

Response regen(Context& ctx, decltype(Contest::id) contest_id) {
    return with_tokens_info(ctx, contest_id, [&](TokensInfo&& ti) {
        auto token_caps = capabilities::contest_entry_token_for(
                ContestEntryTokenKind::NORMAL, ctx.session, ti.caps_contest, ti.cu_mode);
        if (not token_caps.regen) {
            return ctx.response_403();
        }
        if (not ti.token) {
            return ctx.response_400("Contest does not have an entry token");
        }
        auto stmt = ctx.mysql.prepare(
                "UPDATE IGNORE contest_entry_tokens SET token=? WHERE contest_id=?");
        do {
            stmt.bind_and_execute(random_token(), contest_id);
        } while (stmt.affected_rows() == 0);
        return ctx.response_ok();
    });
}

Response delete_(Context& ctx, decltype(Contest::id) contest_id) {
    return with_tokens_info(ctx, contest_id, [&](TokensInfo&& ti) {
        auto token_caps = capabilities::contest_entry_token_for(
                ContestEntryTokenKind::NORMAL, ctx.session, ti.caps_contest, ti.cu_mode);
        if (not token_caps.delete_) {
            return ctx.response_403();
        }
        assert(capabilities::contest_entry_token_for(
                ContestEntryTokenKind::SHORT, ctx.session, ti.caps_contest, ti.cu_mode)
                        .delete_);
        auto stmt = ctx.mysql.prepare("DELETE FROM contest_entry_tokens WHERE contest_id=?");
        stmt.bind_and_execute(contest_id);
        return ctx.response_ok();
    });
}

static void set_or_regen_short_token(mysql::Connection& mysql, decltype(Contest::id) contest_id) {
    auto stmt = mysql.prepare("UPDATE IGNORE contest_entry_tokens SET short_token=?, "
                              "short_token_expiration=? WHERE "
                              "contest_id=? AND (short_token IS NULL or short_token!=?)");
    auto exp_datetime = mysql_date(
            std::chrono::system_clock::now() + ContestEntryToken::short_token_max_lifetime);
    do {
        auto new_short_token = random_short_token();
        stmt.bind_and_execute(new_short_token, exp_datetime, contest_id, new_short_token);
    } while (stmt.affected_rows() == 0);
}

Response add_short(Context& ctx, decltype(Contest::id) contest_id) {
    return with_tokens_info(ctx, contest_id, [&](TokensInfo&& ti) {
        auto token_caps = capabilities::contest_entry_token_for(
                ContestEntryTokenKind::SHORT, ctx.session, ti.caps_contest, ti.cu_mode);
        if (not token_caps.create) {
            return ctx.response_403();
        }
        if (not ti.token) {
            return ctx.response_400("Contest does not have an entry token");
        }
        if (ti.short_token) {
            return ctx.response_400("Contest already has a short entry token");
        }
        set_or_regen_short_token(ctx.mysql, contest_id);
        return ctx.response_ok();
    });
}

Response regen_short(Context& ctx, decltype(Contest::id) contest_id) {
    return with_tokens_info(ctx, contest_id, [&](TokensInfo&& ti) {
        auto token_caps = capabilities::contest_entry_token_for(
                ContestEntryTokenKind::SHORT, ctx.session, ti.caps_contest, ti.cu_mode);
        if (not token_caps.regen) {
            return ctx.response_403();
        }
        if (not ti.short_token) {
            return ctx.response_400("Contest does not have a short entry token");
        }
        set_or_regen_short_token(ctx.mysql, contest_id);
        return ctx.response_ok();
    });
}

Response delete_short(Context& ctx, decltype(Contest::id) contest_id) {
    return with_tokens_info(ctx, contest_id, [&](TokensInfo&& ti) {
        auto token_caps = capabilities::contest_entry_token_for(
                ContestEntryTokenKind::SHORT, ctx.session, ti.caps_contest, ti.cu_mode);
        if (not token_caps.delete_) {
            return ctx.response_403();
        }
        auto stmt = ctx.mysql.prepare("UPDATE contest_entry_tokens SET short_token=NULL, "
                                      "short_token_expiration=NULL WHERE contest_id=?");
        stmt.bind_and_execute(contest_id);
        return ctx.response_ok();
    });
}

Response view_contest_name(Context& ctx, StringView token_or_short_token) {
    return with_token_contest_info(ctx, token_or_short_token, [&](TokenContestInfo&& tci) {
        if (not tci.caps_token.view_contest_name) {
            return ctx.response_403();
        }

        json_str::Object obj;
        obj.prop_obj("contest", [&](auto& obj) {
            obj.prop("id", tci.contest_id);
            obj.prop("name", tci.contest_name);
        });
        return ctx.response_json(std::move(obj).into_str());
    });
}

Response use(Context& ctx, StringView token_or_short_token) {
    return with_token_contest_info(ctx, token_or_short_token, [&](TokenContestInfo&& tci) {
        if (not tci.caps_token.use) {
            return ctx.response_403();
        }

        auto stmt = ctx.mysql.prepare(
                "INSERT IGNORE contest_users(user_id, contest_id, mode) VALUES(?, ?, ?)");
        assert(ctx.session);
        stmt.bind_and_execute(
                ctx.session->user_id, tci.contest_id, EnumVal{ContestUser::Mode::CONTESTANT});
        if (stmt.affected_rows() == 0) {
            return ctx.response_400("You already participate in the contest");
        }
        return ctx.response_ok();
    });
}

} // namespace web_server::contest_entry_tokens::api

#include "../capabilities/contest.hh"
#include "../capabilities/contest_entry_token.hh"
#include "../http/response.hh"
#include "../web_worker/context.hh"
#include "api.hh"

#include <chrono>
#include <optional>
#include <sim/contest_entry_tokens/contest_entry_token.hh>
#include <sim/contest_users/contest_user.hh>
#include <sim/contests/contest.hh>
#include <sim/random.hh>
#include <sim/sql/sql.hh>
#include <simlib/json_str/json_str.hh>
#include <simlib/string_view.hh>
#include <simlib/time.hh>

using sim::contest_entry_tokens::ContestEntryToken;
using sim::contest_users::ContestUser;
using sim::contests::Contest;
using sim::sql::DeleteFrom;
using sim::sql::InsertIgnoreInto;
using sim::sql::InsertInto;
using sim::sql::Select;
using sim::sql::Update;
using web_server::capabilities::ContestEntryTokenKind;
using web_server::http::Response;
using web_server::web_worker::Context;

namespace {

struct TokensInfo {
    decltype(Contest::is_public) contest_is_public;
    std::optional<decltype(ContestUser::mode)> cu_mode;
    std::optional<decltype(ContestEntryToken::token)> token;
    decltype(ContestEntryToken::short_token) short_token;
    decltype(ContestEntryToken::short_token) short_token_real_value;
    decltype(ContestEntryToken::short_token_expiration) short_token_expiration;
    std::string curr_time;
    web_server::capabilities::Contest caps_contest;
};

std::optional<TokensInfo> get_tokens_info_for(Context& ctx, decltype(Contest::id) contest_id) {
    TokensInfo ti;
    auto stmt = ctx.mysql.execute(
        Select("c.is_public, cu.mode, cet.token, cet.short_token, cet.short_token_expiration")
            .from("contests c")
            .left_join("contest_users cu")
            .on(" cu.contest_id=c.id AND cu.user_id=?",
                ctx.session ? std::optional{ctx.session->user_id} : std::nullopt)
            .left_join("contest_entry_tokens cet")
            .on("cet.contest_id=c.id")
            .where("c.id=?", contest_id)
    );
    stmt.res_bind(
        ti.contest_is_public, ti.cu_mode, ti.token, ti.short_token, ti.short_token_expiration
    );
    if (not stmt.next()) {
        return std::nullopt;
    }
    ti.curr_time = utc_mysql_datetime();
    ti.short_token_real_value = ti.short_token;
    throw_assert(ti.short_token.has_value() == ti.short_token_expiration.has_value());
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
    auto ti_opt = get_tokens_info_for(ctx, contest_id);
    if (not ti_opt) {
        return ctx.response_404();
    }
    auto resp = std::forward<Func>(func)(*ti_opt);
    return resp;
}

struct ContestInfo {
    decltype(Contest::id) contest_id;
    decltype(Contest::name) contest_name;
    web_server::capabilities::Contest caps_contest;
    web_server::capabilities::ContestEntryToken caps_token;
};

std::optional<ContestInfo>
get_token_contest_info_for(Context& ctx, StringView token_or_short_token) {
    ContestInfo tci;
    decltype(Contest::is_public) contest_is_public;
    std::optional<decltype(ContestUser::mode)> cu_mode;
    static_assert(
        decltype(ContestEntryToken::token)::len !=
            decltype(ContestEntryToken::short_token)::value_type::len,
        "These cannot be equal because this would cause conflict in selecting the token in the "
        "below query"
    );
    auto token_kind = token_or_short_token.size() == decltype(ContestEntryToken::token)::len
        ? ContestEntryTokenKind::NORMAL
        : ContestEntryTokenKind::SHORT;
    auto stmt = ctx.mysql.execute(
        Select("c.id, c.name, c.is_public, cu.mode")
            .from("contest_entry_tokens cet")
            .left_join("contests c")
            .on("c.id = cet.contest_id")
            .left_join("contest_users cu")
            .on("cu.contest_id=c.id AND cu.user_id=?",
                ctx.session ? std::optional{ctx.session->user_id} : std::nullopt)
            .where(
                "cet.token=? OR (cet.short_token=? AND cet.short_token_expiration>?)",
                token_or_short_token,
                token_or_short_token,
                utc_mysql_datetime()
            )
    );
    stmt.res_bind(tci.contest_id, tci.contest_name, contest_is_public, cu_mode);
    if (not stmt.next()) {
        return std::nullopt;
    }
    tci.caps_contest =
        web_server::capabilities::contest_for(ctx.session, contest_is_public, cu_mode);
    tci.caps_token = web_server::capabilities::contest_entry_token_for(
        token_kind, ctx.session, tci.caps_contest, cu_mode
    );
    return tci;
}

template <class Func>
Response with_token_contest_info(Context& ctx, StringView token_or_short_token, Func&& func) {
    auto tci_opt = get_token_contest_info_for(ctx, token_or_short_token);
    if (not tci_opt) {
        return ctx.response_404();
    }
    auto resp = std::forward<Func>(func)(*tci_opt);
    return resp;
}

auto random_token() { return sim::generate_random_token(decltype(ContestEntryToken::token)::len); }

auto random_short_token() {
    return sim::generate_random_token(decltype(ContestEntryToken::short_token)::value_type::len);
}

void prop_capabilites(
    json_str::ObjectBuilder& obj, const web_server::capabilities::ContestEntryToken& caps
) {
    obj.prop_obj("capabilities", [&](auto& obj) {
        obj.prop("create", caps.create);
        obj.prop("regenerate", caps.regenerate);
        obj.prop("delete", caps.delete_);
    });
}

void prop_token(
    json_str::ObjectBuilder& obj,
    const TokensInfo& ti,
    const web_server::capabilities::ContestEntryToken& caps
) {
    if (caps.view) {
        obj.prop_obj("token", [&](auto& obj) {
            obj.prop("value", ti.token);
            prop_capabilites(obj, caps);
        });
    }
}

void prop_short_token(
    json_str::ObjectBuilder& obj,
    const TokensInfo& ti,
    const web_server::capabilities::ContestEntryToken& caps
) {
    if (caps.view) {
        obj.prop_obj("short_token", [&](auto& obj) {
            obj.prop("value", ti.short_token);
            obj.prop("expires_at", ti.short_token_expiration);
            prop_capabilites(obj, caps);
        });
    }
}

} // namespace

namespace web_server::contest_entry_tokens::api {

Response view(Context& ctx, decltype(Contest::id) contest_id) {
    return with_tokens_info(ctx, contest_id, [&](TokensInfo& ti) {
        if (not ti.caps_contest.entry_tokens.view) {
            return ctx.response_403();
        }

        json_str::Object obj;
        auto token_caps = capabilities::contest_entry_token_for(
            ContestEntryTokenKind::NORMAL, ctx.session, ti.caps_contest, ti.cu_mode
        );
        prop_token(obj, ti, token_caps);
        auto short_token_caps = capabilities::contest_entry_token_for(
            capabilities::ContestEntryTokenKind::SHORT, ctx.session, ti.caps_contest, ti.cu_mode
        );
        prop_short_token(obj, ti, short_token_caps);

        return ctx.response_json(std::move(obj).into_str());
    });
}

Response add(Context& ctx, decltype(Contest::id) contest_id) {
    return with_tokens_info(ctx, contest_id, [&](TokensInfo& ti) {
        auto token_caps = capabilities::contest_entry_token_for(
            ContestEntryTokenKind::NORMAL, ctx.session, ti.caps_contest, ti.cu_mode
        );
        if (not token_caps.create) {
            return ctx.response_403();
        }
        if (ti.token) {
            return ctx.response_400("Contest already has an entry token");
        }

        ti.token = random_token();
        ctx.mysql.execute(
            InsertInto(
                "contest_entry_tokens (token, contest_id, short_token, short_token_expiration)"
            )
                .values("?, ?, NULL, NULL", ti.token, contest_id)
        );
        json_str::Object obj;
        prop_token(obj, ti, token_caps);
        return ctx.response_json(std::move(obj).into_str());
    });
}

Response regenerate(Context& ctx, decltype(Contest::id) contest_id) {
    return with_tokens_info(ctx, contest_id, [&](TokensInfo& ti) {
        auto token_caps = capabilities::contest_entry_token_for(
            ContestEntryTokenKind::NORMAL, ctx.session, ti.caps_contest, ti.cu_mode
        );
        if (not token_caps.regenerate) {
            return ctx.response_403();
        }
        if (not ti.token) {
            return ctx.response_400("Contest does not have an entry token");
        }
        for (;;) {
            auto new_token = random_token();
            if (new_token == ti.token) {
                continue;
            }
            auto stmt = ctx.mysql.execute(Update("contest_entry_tokens")
                                              .set("token=?", new_token)
                                              .where("contest_id=?", contest_id));
            json_str::Object obj;
            ti.token = std::move(new_token);
            prop_token(obj, ti, token_caps);
            return ctx.response_json(std::move(obj).into_str());
        }
    });
}

Response delete_(Context& ctx, decltype(Contest::id) contest_id) {
    return with_tokens_info(ctx, contest_id, [&](TokensInfo& ti) {
        auto token_caps = capabilities::contest_entry_token_for(
            ContestEntryTokenKind::NORMAL, ctx.session, ti.caps_contest, ti.cu_mode
        );
        if (not token_caps.delete_) {
            return ctx.response_403();
        }
        throw_assert(capabilities::contest_entry_token_for(
                         ContestEntryTokenKind::SHORT, ctx.session, ti.caps_contest, ti.cu_mode
        )
                         .delete_);
        ctx.mysql.execute(DeleteFrom("contest_entry_tokens").where("contest_id=?", contest_id));
        return ctx.response_ok();
    });
}

[[nodiscard]] static Response set_or_regen_short_token(
    Context& ctx,
    decltype(Contest::id) contest_id,
    TokensInfo& ti,
    const capabilities::ContestEntryToken& caps
) {
    for (;;) {
        ti.short_token = random_short_token();
        // Make sure the new short token is different than the old short token
        if (ti.short_token == ti.short_token_real_value) {
            continue;
        }

        ti.short_token_expiration = utc_mysql_datetime_with_offset(
            std::chrono::seconds{ContestEntryToken::SHORT_TOKEN_MAX_LIFETIME}.count()
        );
        ctx.mysql.execute(Update("contest_entry_tokens")
                              .set(
                                  "short_token=?, short_token_expiration=?",
                                  ti.short_token,
                                  ti.short_token_expiration
                              )
                              .where("contest_id=?", contest_id));
        json_str::Object obj;
        prop_short_token(obj, ti, caps);
        return ctx.response_json(std::move(obj).into_str());
    }
}

Response add_short(Context& ctx, decltype(Contest::id) contest_id) {
    return with_tokens_info(ctx, contest_id, [&](TokensInfo& ti) {
        auto short_token_caps = capabilities::contest_entry_token_for(
            ContestEntryTokenKind::SHORT, ctx.session, ti.caps_contest, ti.cu_mode
        );
        if (not short_token_caps.create) {
            return ctx.response_403();
        }
        if (not ti.token) {
            return ctx.response_400("Contest does not have an entry token");
        }
        if (ti.short_token) {
            return ctx.response_400("Contest already has a short entry token");
        }
        return set_or_regen_short_token(ctx, contest_id, ti, short_token_caps);
    });
}

Response regenerate_short(Context& ctx, decltype(Contest::id) contest_id) {
    return with_tokens_info(ctx, contest_id, [&](TokensInfo& ti) {
        auto short_token_caps = capabilities::contest_entry_token_for(
            ContestEntryTokenKind::SHORT, ctx.session, ti.caps_contest, ti.cu_mode
        );
        if (not short_token_caps.regenerate) {
            return ctx.response_403();
        }
        if (not ti.short_token) {
            return ctx.response_400("Contest does not have a short entry token");
        }
        return set_or_regen_short_token(ctx, contest_id, ti, short_token_caps);
    });
}

Response delete_short(Context& ctx, decltype(Contest::id) contest_id) {
    return with_tokens_info(ctx, contest_id, [&](TokensInfo& ti) {
        auto short_token_caps = capabilities::contest_entry_token_for(
            ContestEntryTokenKind::SHORT, ctx.session, ti.caps_contest, ti.cu_mode
        );
        if (not short_token_caps.delete_) {
            return ctx.response_403();
        }
        ctx.mysql.execute(Update("contest_entry_tokens")
                              .set("short_token=NULL, short_token_expiration=NULL")
                              .where("contest_id=?", contest_id));
        return ctx.response_ok();
    });
}

Response view_contest_name(Context& ctx, StringView token_or_short_token) {
    return with_token_contest_info(ctx, token_or_short_token, [&](ContestInfo& ci) {
        if (not ci.caps_token.view_contest_name) {
            return ctx.response_403();
        }

        json_str::Object obj;
        obj.prop_obj("contest", [&](auto& obj) {
            obj.prop("id", ci.contest_id);
            obj.prop("name", ci.contest_name);
        });
        return ctx.response_json(std::move(obj).into_str());
    });
}

Response use(Context& ctx, StringView token_or_short_token) {
    return with_token_contest_info(ctx, token_or_short_token, [&](ContestInfo& ci) {
        if (not ci.caps_token.use) {
            return ctx.response_403();
        }

        throw_assert(ctx.session);
        auto stmt = ctx.mysql.execute(
            InsertIgnoreInto("contest_users (user_id, contest_id, mode)")
                .values(
                    "?, ?, ?", ctx.session->user_id, ci.contest_id, ContestUser::Mode::CONTESTANT
                )
        );
        if (stmt.affected_rows() == 0) {
            return ctx.response_400("You already participate in the contest");
        }
        return ctx.response_ok();
    });
}

} // namespace web_server::contest_entry_tokens::api

#pragma once

#include "../http/response.hh"
#include "../web_worker/context.hh"

#include <sim/contests/contest.hh>
#include <simlib/string_view.hh>

namespace web_server::contest_entry_tokens::api {

http::Response view(web_worker::Context& ctx, decltype(sim::contests::Contest::id) contest_id);

http::Response add(web_worker::Context& ctx, decltype(sim::contests::Contest::id) contest_id);

http::Response regen(web_worker::Context& ctx, decltype(sim::contests::Contest::id) contest_id);

http::Response delete_(web_worker::Context& ctx, decltype(sim::contests::Contest::id) contest_id);

http::Response add_short(web_worker::Context& ctx, decltype(sim::contests::Contest::id) contest_id);

http::Response
regen_short(web_worker::Context& ctx, decltype(sim::contests::Contest::id) contest_id);

http::Response
delete_short(web_worker::Context& ctx, decltype(sim::contests::Contest::id) contest_id);

http::Response view_contest_name(web_worker::Context& ctx, StringView token_or_short_token);

http::Response use(web_worker::Context& ctx, StringView token_or_short_token);

} // namespace web_server::contest_entry_tokens::api

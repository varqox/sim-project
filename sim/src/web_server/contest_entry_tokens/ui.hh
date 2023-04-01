#pragma once

#include "../http/response.hh"
#include "../web_worker/context.hh"

#include <simlib/string_view.hh>

namespace web_server::contest_entry_tokens::ui {

http::Response enter_contest(web_worker::Context& ctx, StringView token_or_short_token);

} // namespace web_server::contest_entry_tokens::ui

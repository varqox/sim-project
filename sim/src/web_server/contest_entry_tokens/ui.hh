#pragma once

#include "simlib/string_view.hh"
#include "src/web_server/http/response.hh"
#include "src/web_server/web_worker/context.hh"

namespace web_server::contest_entry_tokens {

http::Response enter_contest(web_worker::Context& ctx, StringView token_or_short_token);

} // namespace web_server::contest_entry_tokens

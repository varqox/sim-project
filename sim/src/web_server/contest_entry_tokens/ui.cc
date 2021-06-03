#include "src/web_server/contest_entry_tokens/ui.hh"
#include "simlib/string_view.hh"
#include "src/web_server/http/response.hh"
#include "src/web_server/web_worker/context.hh"

using web_server::http::Response;
using web_server::web_worker::Context;

namespace web_server::contest_entry_tokens {

Response enter_contest(Context& ctx, StringView token_or_short_token) {
    auto js = concat("enter_contest_using_token(false, '", token_or_short_token, "')");
    return ctx.response_ui("Enter contest", js);
}

} // namespace web_server::contest_entry_tokens

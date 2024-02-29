#include "../http/response.hh"
#include "../web_worker/context.hh"
#include "ui.hh"

#include <simlib/string_view.hh>

using web_server::http::Response;
using web_server::web_worker::Context;

namespace web_server::contest_entry_tokens::ui {

Response enter_contest(Context& ctx, StringView token_or_short_token) {
    auto js = concat("enter_contest_using_token(false, '", token_or_short_token, "')");
    return ctx.response_ui("Enter contest", js);
}

} // namespace web_server::contest_entry_tokens::ui

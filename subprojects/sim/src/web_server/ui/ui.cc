#include "../http/response.hh"
#include "../web_worker/context.hh"
#include "simlib/string_view.hh"
#include "ui.hh"

using web_server::http::Response;
using web_server::web_worker::Context;

namespace web_server::ui {

Response scripts_js(Context& ctx, StringView /*timestamp*/) {
    return ctx.response_file("static/kit/scripts.js", "text/javascript; charset=utf-8");
}

Response jquery_js(Context& ctx, StringView /*timestamp*/) {
    return ctx.response_file("static/kit/jquery.js", "text/javascript; charset=utf-8");
}

Response styles_css(Context& ctx, StringView /*timestamp*/) {
    return ctx.response_file("static/kit/styles.css", "text/css; charset=utf-8");
}

Response favicon_ico(Context& ctx) {
    return ctx.response_file("static/favicon.ico", "image/x-icon");
}

} // namespace web_server::ui

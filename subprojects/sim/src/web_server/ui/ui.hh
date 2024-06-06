#pragma once

#include "../http/response.hh"
#include "../web_worker/context.hh"

#include <simlib/string_view.hh>

namespace web_server::ui {

http::Response scripts_js(web_worker::Context& ctx, StringView /*timestamp*/);

http::Response jquery_js(web_worker::Context& ctx, StringView /*timestamp*/);

http::Response styles_css(web_worker::Context& ctx, StringView /*timestamp*/);

http::Response favicon_ico(web_worker::Context& ctx);

} // namespace web_server::ui

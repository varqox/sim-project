#include "../http/response.hh"
#include "../web_worker/context.hh"
#include "ui.hh"

#include <simlib/string_view.hh>

using web_server::http::Response;
using web_server::web_worker::Context;

namespace {

Response&& with_public_cache_valid_for_a_year_and_non_obligator_revalidation(Response&& resp) {
    resp.set_cache(true, 365 * 24 * 60 * 60, false);
    return std::move(resp);
}

} // namespace

namespace web_server::ui {

Response scripts_js(Context& ctx, StringView /*timestamp*/) {
    return with_public_cache_valid_for_a_year_and_non_obligator_revalidation(
        ctx.response_file("static/kit/scripts.js", "text/javascript; charset=utf-8")
    );
}

Response jquery_js(Context& ctx, StringView /*timestamp*/) {
    return with_public_cache_valid_for_a_year_and_non_obligator_revalidation(
        ctx.response_file("static/kit/jquery.js", "text/javascript; charset=utf-8")
    );
}

Response styles_css(Context& ctx, StringView /*timestamp*/) {
    return with_public_cache_valid_for_a_year_and_non_obligator_revalidation(
        ctx.response_file("static/kit/styles.css", "text/css; charset=utf-8")
    );
}

Response favicon_ico(Context& ctx) {
    return with_public_cache_valid_for_a_year_and_non_obligator_revalidation(
        ctx.response_file("static/favicon.ico", "image/x-icon")
    );
}

} // namespace web_server::ui

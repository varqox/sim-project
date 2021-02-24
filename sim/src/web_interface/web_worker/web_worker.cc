#include "web_worker.hh"
#include "simlib/mysql.hh"
#include "simlib/string_view.hh"
#include "src/web_interface/http_request.hh"
#include "src/web_interface/http_response.hh"
#include "src/web_interface/web_worker/context.hh"

#include <optional>
#include <type_traits>

using server::HttpResponse;

namespace sim::web_worker {

WebWorker::WebWorker(MySQL::Connection& mysql)
: mysql{mysql} {
    #define GET(func, url, ...) { static constexpr char url_var[] = url; add_get_handler<url_var, ## __VA_ARGS__>(func); }
    #define POST(func, url, ...) { static constexpr char url_var[] = url; add_post_handler<url_var, ## __VA_ARGS__>(func); }
    // Handlers
    #undef POST
    #undef GET
    // Ensure fast query dispatch
    assert(get_dispatcher.all_potential_collisions().empty());
    assert(post_dispatcher.all_potential_collisions().empty());
}

std::variant<HttpResponse, server::HttpRequest> WebWorker::handle(server::HttpRequest req) {
    request = std::move(req);
    auto& dispatcher = [&]() -> auto& {
        switch (request->method) {
        case server::HttpRequest::HEAD:
        case server::HttpRequest::GET: return get_dispatcher;
        case server::HttpRequest::POST: return post_dispatcher;
        }
        __builtin_unreachable();
    }
    ();
    auto resp = dispatcher.dispatch(request->target);
    if (resp) {
        return *resp;
    }
    return std::move(*request);
}

template <class ResponseMaker>
HttpResponse WebWorker::handler_impl(ResponseMaker&& response_maker) {
    static_assert(std::is_invocable_r_v<HttpResponse, ResponseMaker&&, Context&>);
    auto ctx = Context{
        .request = request.value(),
        .mysql = mysql,
        .session = std::nullopt,
        .cookie_changes = {},
    };
    ctx.open_session();
    auto response = std::forward<ResponseMaker>(response_maker)(ctx);
    assert(ctx.cookie_changes.cookies_as_headers.is_empty());
    if (ctx.session) {
        ctx.close_session();
    }
    return response;
}

template <const char* url_pattern, auto... CustomParsers, class... Params>
void WebWorker::do_add_get_handler(
    strongly_typed_function<HttpResponse(Context&, Params...)> handler) {
    get_dispatcher.add_handler<url_pattern, CustomParsers...>(
        [&, handler = std::move(handler)](Params... args) {
            return handler_impl(
                [&](Context& ctx) { return handler(ctx, std::forward<Params>(args)...); });
        });
}

template <const char* url_pattern, auto... CustomParsers, class... Params>
void WebWorker::do_add_post_handler(
    strongly_typed_function<HttpResponse(Context&, Params...)> handler) {
    post_dispatcher.add_handler<url_pattern, CustomParsers...>(
        [&, handler = std::move(handler)](Params... args) {
            return handler_impl([&](Context& ctx) {
                // First check the CSRF token, if no session is open then we use value from
                // cookie to pass the verification
                StringView csrf_token = ctx.session
                    ? StringView{ctx.session->csrf_token}
                    : ctx.request.get_cookie(Context::Session::csrf_token_cookie_name);
                if (ctx.request.form_fields.get_or("csrf_token", "") != csrf_token) {
                    return ctx.response_400("Invalid CSRF token");
                }
                // Safe to pass the request to the proper handler
                return handler(ctx, std::forward<Params>(args)...);
            });
        });
}

} // namespace sim::web_worker

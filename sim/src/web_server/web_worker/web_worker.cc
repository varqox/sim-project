#include "web_worker.hh"
#include "simlib/mysql/mysql.hh"
#include "simlib/string_view.hh"
#include "src/web_server/http/request.hh"
#include "src/web_server/http/response.hh"
#include "src/web_server/web_worker/context.hh"

#include <optional>
#include <type_traits>

using web_server::http::Request;
using web_server::http::Response;

namespace web_server::web_worker {

#define GET(func, url, ...)                            \
    {                                                  \
        static constexpr char url_var[] = url;         \
        add_get_handler<url_var, ##__VA_ARGS__>(func); \
    }
#define POST(func, url, ...)                            \
    {                                                   \
        static constexpr char url_var[] = url;          \
        add_post_handler<url_var, ##__VA_ARGS__>(func); \
    }
WebWorker::WebWorker(mysql::Connection& mysql)
: mysql{mysql} {
    // Handlers
    // Ensure fast query dispatch
    assert(get_dispatcher.all_potential_collisions().empty());
    assert(post_dispatcher.all_potential_collisions().empty());
}
#undef POST
#undef GET

std::variant<Response, Request> WebWorker::handle(Request req) {
    request = std::move(req);
    auto& dispatcher = [&]() -> auto& {
        switch (request->method) {
        case Request::HEAD:
        case Request::GET: return get_dispatcher;
        case Request::POST: return post_dispatcher;
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
Response WebWorker::handler_impl(ResponseMaker&& response_maker) {
    static_assert(std::is_invocable_r_v<Response, ResponseMaker&&, Context&>);
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
    strongly_typed_function<Response(Context&, Params...)> handler) {
    get_dispatcher.add_handler<url_pattern, CustomParsers...>(
        [&, handler = std::move(handler)](Params... args) {
            return handler_impl(
                [&](Context& ctx) { return handler(ctx, std::forward<Params>(args)...); });
        });
}

template <const char* url_pattern, auto... CustomParsers, class... Params>
void WebWorker::do_add_post_handler(
    strongly_typed_function<Response(Context&, Params...)> handler) {
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

} // namespace web_server::web_worker

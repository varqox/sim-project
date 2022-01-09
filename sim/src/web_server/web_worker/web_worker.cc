#include "web_worker.hh"
#include "sim/jobs/utils.hh"
#include "simlib/mysql/mysql.hh"
#include "simlib/string_view.hh"
#include "src/web_server/contest_entry_tokens/api.hh"
#include "src/web_server/contest_entry_tokens/ui.hh"
#include "src/web_server/http/request.hh"
#include "src/web_server/http/response.hh"
#include "src/web_server/users/api.hh"
#include "src/web_server/users/ui.hh"
#include "src/web_server/web_worker/context.hh"

#include <optional>
#include <type_traits>

using web_server::http::Request;
using web_server::http::Response;

namespace web_server::web_worker {

#define GET(url, ...)                          \
    {                                          \
        static constexpr char url_val[] = url; \
        add_get_handler<url_val, ##__VA_ARGS__> REST
#define POST(url, ...)                         \
    {                                          \
        static constexpr char url_val[] = url; \
        add_post_handler<url_val, ##__VA_ARGS__> REST
#define REST(func) \
    (func);        \
    }

WebWorker::WebWorker(mysql::Connection& mysql)
: mysql{mysql} {
    // Handlers
    // clang-format off
    GET("/api/contest/{u64}/entry_tokens")(contest_entry_tokens::api::view);
    GET("/api/contest_entry_token/{string}/contest_name")(contest_entry_tokens::api::view_contest_name);
    GET("/api/user/{u64}")(users::api::view);
    GET("/api/users")(users::api::list);
    GET("/api/users/id%3E/{u64}")(users::api::list_above_id);
    GET("/api/users/type=/{string}")(users::api::list_by_type);
    GET("/api/users/type=/{string}/id%3E/{u64}")(users::api::list_by_type_above_id);
    GET("/enter_contest/{string}")(contest_entry_tokens::ui::enter_contest);
    GET("/sign_in")(users::ui::sign_in);
    GET("/sign_out")(users::ui::sign_out);
    GET("/sign_up")(users::ui::sign_up);
    GET("/user/{u64}/change_password")(users::ui::change_password);
    GET("/user/{u64}/delete")(users::ui::delete_);
    GET("/user/{u64}/edit")(users::ui::edit);
    GET("/users")(users::ui::list);
    GET("/users/add")(users::ui::add);
    POST("/api/contest/{u64}/entry_tokens/add")(contest_entry_tokens::api::add);
    POST("/api/contest/{u64}/entry_tokens/add_short")(contest_entry_tokens::api::add_short);
    POST("/api/contest/{u64}/entry_tokens/delete")(contest_entry_tokens::api::delete_);
    POST("/api/contest/{u64}/entry_tokens/delete_short")(contest_entry_tokens::api::delete_short);
    POST("/api/contest/{u64}/entry_tokens/regen")(contest_entry_tokens::api::regen);
    POST("/api/contest/{u64}/entry_tokens/regen_short")(contest_entry_tokens::api::regen_short);
    POST("/api/contest_entry_token/{string}/use")(contest_entry_tokens::api::use);
    POST("/api/sign_in")(users::api::sign_in);
    POST("/api/sign_out")(users::api::sign_out);
    POST("/api/sign_up")(users::api::sign_up);
    POST("/api/user/{u64}/change_password")(users::api::change_password);
    POST("/api/user/{u64}/delete")(users::api::delete_);
    POST("/api/user/{u64}/edit")(users::api::edit);
    POST("/api/users/add")(users::api::add);
    // clang-format on
    // Ensure fast query dispatch
    assert(get_dispatcher.all_potential_collisions().empty());
    assert(post_dispatcher.all_potential_collisions().empty());
}
#undef GET
#undef POST
#undef REST

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
    auto transaction = ctx.mysql.start_transaction();
    ctx.open_session();
    auto response = std::forward<ResponseMaker>(response_maker)(ctx);
    assert(ctx.cookie_changes.cookies_as_headers.is_empty() and
            "cookie_changes need to be handled during producing the response");
    if (ctx.session) {
        ctx.close_session();
    }
    transaction.commit();
    if (ctx.notify_job_server_after_commit) {
        sim::jobs::notify_job_server();
    }
    return response;
}

template <const char* url_pattern, auto... CustomParsers, class... Params>
void WebWorker::do_add_get_handler(
        strongly_typed_function<Response(Context&, Params...)> handler) {
    get_dispatcher.add_handler<url_pattern, CustomParsers...>(
            [&, handler = std::move(handler)](Params... args) {
                return handler_impl([&](Context& ctx) {
                    return handler(ctx, std::forward<Params>(args)...);
                });
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
                    if (ctx.request.headers.get("x-csrf-token") != csrf_token) {
                        return ctx.response_400("Missing or invalid CSRF token");
                    }
                    // Safe to pass the request to the proper handler
                    return handler(ctx, std::forward<Params>(args)...);
                });
            });
}

} // namespace web_server::web_worker

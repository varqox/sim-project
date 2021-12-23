#pragma once

#include "sim/mysql/mysql.hh"
#include "sim/sessions/session.hh"
#include "simlib/http/url_dispatcher.hh"
#include "src/web_server/http/cookies.hh"
#include "src/web_server/http/request.hh"
#include "src/web_server/http/response.hh"
#include "src/web_server/web_worker/context.hh"

#include <variant>

namespace web_server::web_worker {

class WebWorker {
    using UrlDispatcher = ::http::UrlDispatcher<http::Response>;
    mysql::Connection& mysql;
    std::optional<http::Request> request;
    UrlDispatcher get_dispatcher;
    UrlDispatcher post_dispatcher;

public:
    explicit WebWorker(mysql::Connection& mysql);

    // Returns response for @p request or @p request if it cannot handle the @p request
    std::variant<http::Response, http::Request> handle(http::Request req);

private:
    template <class ResponseMaker>
    http::Response handler_impl(ResponseMaker&& response_maker);

    template <const char* url_pattern, auto... CustomParsers, class... Params>
    void do_add_get_handler(
            strongly_typed_function<http::Response(Context&, Params...)> handler);

    template <const char* url_pattern, auto... CustomParsers, class... Params>
    void do_add_post_handler(
            strongly_typed_function<http::Response(Context&, Params...)> handler);

    template <const char* url_pattern, auto... CustomParsers>
    void add_get_handler(UrlDispatcher::HandlerImpl<std::tuple<Context&>, std::tuple<>,
            url_pattern, CustomParsers...>
                    handler) {
        do_add_get_handler<url_pattern, CustomParsers...>(std::move(handler));
    }

    template <const char* url_pattern, auto... CustomParsers>
    void add_post_handler(UrlDispatcher::HandlerImpl<std::tuple<Context&>, std::tuple<>,
            url_pattern, CustomParsers...>
                    handler) {
        do_add_post_handler<url_pattern, CustomParsers...>(std::move(handler));
    }
};

} // namespace web_server::web_worker

#pragma once

#include "sim/mysql.hh"
#include "sim/session.hh"
#include "simlib/http/url_dispatcher.hh"
#include "src/web_interface/http_cookies.hh"
#include "src/web_interface/http_request.hh"
#include "src/web_interface/http_response.hh"
#include "src/web_interface/web_worker/context.hh"

#include <variant>

namespace sim::web_worker {

class WebWorker {
    using UrlDispatcher = http::UrlDispatcher<server::HttpResponse>;
    MySQL::Connection& mysql;
    std::optional<server::HttpRequest> request;
    UrlDispatcher get_dispatcher;
    UrlDispatcher post_dispatcher;

public:
    explicit WebWorker(MySQL::Connection& mysql);

    // Returns response for @p request or @p request if it cannot handle the @p request
    std::variant<server::HttpResponse, server::HttpRequest> handle(server::HttpRequest req);

private:
    template <class ResponseMaker>
    server::HttpResponse handler_impl(ResponseMaker&& response_maker);

    template <const char* url_pattern, auto... CustomParsers, class... Params>
    void do_add_get_handler(
        strongly_typed_function<server::HttpResponse(Context&, Params...)> handler);

    template <const char* url_pattern, auto... CustomParsers, class... Params>
    void do_add_post_handler(
        strongly_typed_function<server::HttpResponse(Context&, Params...)> handler);

    template <const char* url_pattern, auto... CustomParsers>
    void add_get_handler(UrlDispatcher::HandlerImpl<
                         std::tuple<Context&>, std::tuple<>, url_pattern, CustomParsers...>
                             handler) {
        do_add_get_handler<url_pattern, CustomParsers...>(std::move(handler));
    }

    template <const char* url_pattern, auto... CustomParsers>
    void add_post_handler(UrlDispatcher::HandlerImpl<
                          std::tuple<Context&>, std::tuple<>, url_pattern, CustomParsers...>
                              handler) {
        do_add_post_handler<url_pattern, CustomParsers...>(std::move(handler));
    }
};

} // namespace sim::web_worker

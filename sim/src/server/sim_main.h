#pragma once

#include "http_request.h"
#include "http_response.h"

namespace sim {

server::HttpResponse main(const server::HttpRequest& req);
server::HttpResponse mainPage(const server::HttpRequest&);
server::HttpResponse getStaticFile(const server::HttpRequest& req);

} // namespace sim

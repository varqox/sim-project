#pragma once

#include "sim/http/form_fields.hh"
#include "src/web_interface/http_headers.hh"

#include <optional>

namespace server {

class HttpRequest {
public:
    enum Method : uint8_t { GET, POST, HEAD } method = GET;
    HttpHeaders headers;
    std::string target, http_version, content;
    http::FormFields form_fields;

    HttpRequest() = default;

    HttpRequest(const HttpRequest&) = default;
    HttpRequest(HttpRequest&&) noexcept = default;
    HttpRequest& operator=(const HttpRequest&) = default;
    HttpRequest& operator=(HttpRequest&&) = default;

    ~HttpRequest();

    StringView get_cookie(StringView name) const noexcept;
};

} // namespace server

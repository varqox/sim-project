#pragma once

#include "form_fields.hh"
#include "headers.hh"

#include <optional>

namespace web_server::http {

class Request {
public:
    enum Method : uint8_t { GET, POST, HEAD } method = GET;

    Headers headers;
    std::string target, http_version, content;
    FormFields form_fields;

    Request() = default;

    Request(const Request&) = default;
    Request(Request&&) noexcept = default;
    Request& operator=(const Request&) = default;
    Request& operator=(Request&&) = default;

    ~Request();

    StringView get_cookie(StringView name) const noexcept;
};

} // namespace web_server::http

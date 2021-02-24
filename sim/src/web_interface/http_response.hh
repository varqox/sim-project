#pragma once

#include "simlib/string_view.hh"
#include "simlib/time.hh"
#include "src/web_interface/http_cookies.hh"
#include "src/web_interface/http_headers.hh"

namespace server {

class HttpResponse {
public:
    enum ContentType : uint8_t { TEXT, FILE, FILE_TO_REMOVE } content_type;
    InplaceBuff<100> status_code;
    HttpHeaders headers{};
    HttpCookies cookies{};
    InplaceBuff<4096> content{};

    explicit HttpResponse(ContentType con_type = TEXT, StringView stat_code = "200 OK")
    : content_type(con_type)
    , status_code(stat_code) {}

    HttpResponse(const HttpResponse&) = default;
    HttpResponse(HttpResponse&&) noexcept = default;
    HttpResponse& operator=(const HttpResponse&) = default;
    HttpResponse& operator=(HttpResponse&&) = default;

    ~HttpResponse() = default;

    void set_cache(bool to_public, uint max_age, bool must_revalidate) {
        headers["expires"] = date("%a, %d %b %Y %H:%M:%S GMT", time(nullptr) + max_age);
        headers["cache-control"] = concat_tostr(
            (to_public ? "public" : "private"), (must_revalidate ? "; must-revalidate" : ""),
            "; max-age=", max_age);
    }
};

} // namespace server

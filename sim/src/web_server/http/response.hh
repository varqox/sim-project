#pragma once

#include "cookies.hh"
#include "headers.hh"
#include <simlib/string_view.hh>

namespace web_server::http {

class Response {
public:
    enum ContentType : uint8_t { TEXT, FILE, FILE_TO_REMOVE } content_type;
    InplaceBuff<100> status_code;
    Headers headers{};
    Cookies cookies{};
    InplaceBuff<4096> content{};

    explicit Response(ContentType con_type = TEXT, StringView stat_code = "200 OK")
    : content_type(con_type)
    , status_code(stat_code) {}

    Response(const Response&) = default;
    Response(Response&&) noexcept = default;
    Response& operator=(const Response&) = default;
    Response& operator=(Response&&) = default;

    ~Response() = default;

    void set_cache(bool to_public, uint max_age, bool must_revalidate);
};

} // namespace web_server::http

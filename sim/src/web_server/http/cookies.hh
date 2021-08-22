#pragma once

#include "simlib/string_view.hh"
#include "src/web_server/http/headers.hh"

#include <optional>

namespace web_server::http {

struct Cookies {
    Headers cookies_as_headers;

    void
    set(StringView name, StringView val, std::optional<time_t> expire,
        std::optional<StringView> path, bool http_only, bool secure);

    std::optional<StringView> get(StringView name) const noexcept {
        if (auto cookie = cookies_as_headers.get(name); cookie) {
            return cookie->substr(0, cookie->find(';'));
        }
        return std::nullopt;
    }
};

} // namespace web_server::http

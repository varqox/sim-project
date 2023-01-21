#include "request.hh"
#include <simlib/debug.hh>
#include <simlib/file_manip.hh>

using std::string;

namespace web_server::http {

Request::~Request() {
    for (auto const& [_, file_path] : form_fields.files()) {
        (void)unlink(file_path);
    }
}

StringView Request::get_cookie(StringView name) const noexcept {
    STACK_UNWINDING_MARK;

    CStringView cookie = headers.get("cookie").value_or("");
    for (size_t beg = 0; beg < cookie.size();) {
        if (cookie[beg] == ' ' && beg + 1 < cookie.size()) {
            ++beg;
        }

        if (0 == cookie.compare(beg, name.size(), name) && beg + name.size() < cookie.size() &&
                cookie[beg + name.size()] == '=')
        {
            beg += name.size() + 1;
            size_t next = std::min(cookie.size(), find(cookie, ';', beg));
            return cookie.substr(beg, next - beg);
        }

        beg = find(cookie, ';', beg);
        if (beg < cookie.size()) {
            ++beg;
        }
    }

    return ""; // Not found
}

} // namespace web_server::http

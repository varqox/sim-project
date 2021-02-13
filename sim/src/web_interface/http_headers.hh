#pragma once

#include <map>
#include <simlib/string_compare.hh>
#include <simlib/string_transform.hh>
#include <string>

namespace server {

class HttpHeaders {
    struct Comparator : public LowerStrCompare {
        struct is_transparent;
    };

    // header name => header value
    std::map<std::string, std::string, Comparator> entries_;

public:
    HttpHeaders() = default;
    HttpHeaders(const HttpHeaders&) = default;
    HttpHeaders(HttpHeaders&&) noexcept = default;
    HttpHeaders& operator=(const HttpHeaders&) = default;
    HttpHeaders& operator=(HttpHeaders&&) noexcept = default;
    ~HttpHeaders() = default;

    std::string& operator[](std::string&& key) { return entries_[std::move(key)]; }

    template <class Key>
    std::string& operator[](Key&& key) {
        StringView strkey = std::forward<Key>(key);
        auto it = entries_.find(strkey);
        if (it == entries_.end()) {
            return entries_[strkey.to_string()];
        }
        return it->second;
    }

    CStringView get(StringView key) const noexcept {
        auto it = entries_.find(key);
        if (it == entries_.end()) {
            return "";
        }
        return it->second;
    }

    [[nodiscard]] auto begin() noexcept { return entries_.begin(); }
    [[nodiscard]] auto begin() const noexcept { return entries_.begin(); }
    [[nodiscard]] auto end() noexcept { return entries_.end(); }
    [[nodiscard]] auto end() const noexcept { return entries_.end(); }

    void clear() noexcept { entries_.clear(); }
};

} // namespace server

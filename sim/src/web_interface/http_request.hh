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

    class Form {
    public:
        // name (the one from a HTML form) => tmp file's path
        std::map<std::string, std::string, std::less<>> files;
        // name => value; for a file: name => client_filename
        std::map<std::string, std::string, std::less<>> other;

        Form() = default;

        Form(const Form&) = default;
        Form(Form&&) noexcept = default;
        Form& operator=(const Form&) = default;
        Form& operator=(Form&&) noexcept = default;

        ~Form();

        explicit operator auto &() noexcept { return other; }

        std::string& operator[](std::string&& key) { return other[std::move(key)]; }

        template <class Key>
        std::string& operator[](Key&& key) {
            StringView strkey = std::forward<Key>(key);
            auto it = other.find(strkey);
            if (it == other.end()) {
                return other[strkey.to_string()];
            }
            return it->second;
        }

        /// @brief Returns value of the variable @p name or empty string if such
        /// does not exist
        CStringView get(StringView name) const noexcept {
            auto it = other.find(name);
            if (it == other.end()) {
                return "";
            }
            return it->second;
        }

        std::optional<CStringView> get_opt(StringView key) const noexcept {
            auto it = other.find(key);
            if (it == other.end()) {
                return std::nullopt;
            }
            return it->second;
        }

        bool exist(StringView name) const noexcept { return other.find(name) != other.end(); }

        /// @brief Returns path of the uploaded file with the form's name
        /// @p name or empty string if such does not exist
        CStringView file_path(StringView name) const noexcept {
            auto it = files.find(name);
            if (it == files.end()) {
                return "";
            }
            return it->second;
        }
    } form_data;

    HttpRequest() = default;

    HttpRequest(const HttpRequest&) = default;
    HttpRequest(HttpRequest&&) noexcept = default;
    HttpRequest& operator=(const HttpRequest&) = default;
    HttpRequest& operator=(HttpRequest&&) = default;

    ~HttpRequest() = default;

    StringView get_cookie(StringView name) const noexcept;
};

} // namespace server

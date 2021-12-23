#pragma once

#include "simlib/string_view.hh"

#include <map>
#include <optional>
#include <string>

namespace web_server::http {

class FormFields {
    std::map<std::string, std::string, std::less<>>
            files_; // name (the one from HTTP form) => tmp_filename
    std::map<std::string, std::string, std::less<>>
            others_; // name => value (for a file: name => client_filename)

public:
    FormFields() = default;
    ~FormFields() = default;

    FormFields(const FormFields&) = default;
    FormFields(FormFields&&) noexcept = default;
    FormFields& operator=(const FormFields&) = default;
    FormFields& operator=(FormFields&&) noexcept = default;

    // Adds new field or replaces existing one
    void add_field(std::string name, std::string value,
            std::optional<std::string> tmp_file_path = std::nullopt) {
        if (tmp_file_path) {
            files_.emplace(name, std::move(*tmp_file_path));
        } else {
            files_.erase(name);
        }
        others_.emplace(std::move(name), std::move(value));
    }

    /// Returns value of the variable @p name if exists. Returned value may be invalided by
    /// modifying this object
    std::optional<CStringView> get(StringView name) const noexcept {
        auto it = others_.find(name);
        if (it != others_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    bool contains(StringView name) const noexcept { return others_.count(name) == 1; }

    /// Returns path of the uploaded file with the form's name @p name if exists. Returned
    /// value may be invalided by modifying this object.
    std::optional<CStringView> file_path(StringView name) const noexcept {
        auto it = files_.find(name);
        if (it != files_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    const auto& files() const noexcept { return files_; }
};

} // namespace web_server::http

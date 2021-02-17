#pragma once

#include "simlib/string_view.hh"

#include <map>
#include <string>

namespace http {

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
    void add_field(
        std::string name, std::string value,
        std::optional<std::string> tmp_file_path = std::nullopt) {
        if (tmp_file_path) {
            files_.emplace(name, std::move(*tmp_file_path));
        } else {
            files_.erase(name);
        }
        others_.emplace(std::move(name), std::move(value));
    }

    /// @brief Returns value of the variable @p name or nullptr if such does not exist.
    // Returned pointer may be invalided by modifying this object
    const std::string* get(StringView name) const noexcept {
        auto it = others_.find(name);
        return it != others_.end() ? &it->second : nullptr;
    }

    /// @brief Returns value of the variable @p name or @p or_value if such does not exist.
    // Returned pointer may be invalided by modifying this object
    CStringView get_or(StringView name, CStringView or_value) const noexcept {
        auto* val = get(name);
        return val ? *val : or_value;
    }

    bool contains(StringView name) const noexcept { return others_.count(name) == 1; }

    /// @brief Returns path of the uploaded file with the form's name @p name or nullptr if
    /// such does not exist. Returned pointer may be invalided by modifying this object
    const std::string* file_path(StringView name) const noexcept {
        auto it = files_.find(name);
        return it != files_.end() ? &it->second : nullptr;
    }

    const auto& files() const noexcept { return files_; }
};

} // namespace http

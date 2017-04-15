#pragma once

#include "http_headers.h"

namespace server {

class HttpRequest {
public:
	enum Method : uint8_t { GET, POST, HEAD } method = GET;
	HttpHeaders headers;
	std::string target, http_version, content;

	class Form {
	public:
		// files: name (this from form) => tmp_filename
		// other: name => value; for file: name => client_filename
		std::map<std::string, std::string> files, other;

		explicit operator std::map<std::string, std::string>&() {
			return other;
		}

		std::string& operator[](const std::string& key) { return other[key]; }

		/// @brief Returns value of the variable @p name or empty string if such
		/// does not exist
		StringView get(const std::string& name) {
			auto it = other.find(name);
			return (it == other.end() ? StringView{} : it->second);
		}

		bool exist(const std::string& name) const {
			return (other.find(name) != other.end());
		}

		/// @brief Returns path of the uploaded file with the form's name
		/// @p name or empty string if such does not exist
		StringView file_path(const std::string& name) {
			auto it = files.find(name);
			return (it == other.end() ? "" : it->second);
		}

		Form() = default;

		Form(const Form&) = default;
		Form(Form&&) noexcept = default;
		Form& operator=(const Form&) = default;
		Form& operator=(Form&&) noexcept = default;

		~Form();
	} form_data;

	HttpRequest() = default;

	HttpRequest(const HttpRequest&) = default;
	HttpRequest(HttpRequest&&) noexcept = default;
	HttpRequest& operator=(const HttpRequest&) = default;
	HttpRequest& operator=(HttpRequest&&) = default;

	~HttpRequest() {}

	std::string getCookie(const std::string& name) const;
};

} // namespace server

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
		AVLDictMap<std::string, std::string> files, other;

		explicit operator AVLDictMap<std::string, std::string>&() {
			return other;
		}

		std::string& operator[](std::string key) {
			return other[std::move(key)];
		}

		/// @brief Returns value of the variable @p name or empty string if such
		/// does not exist
		CStringView get(StringView name) const noexcept {
			auto it = other.find(name);
			return (it ? it->second : CStringView());
		}

		bool exist(StringView name) const noexcept { return other.find(name); }

		/// @brief Returns path of the uploaded file with the form's name
		/// @p name or empty string if such does not exist
		CStringView file_path(StringView name) const noexcept {
			auto it = files.find(name);
			return (it ? it->second : CStringView());
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

	StringView getCookie(StringView name) const noexcept;
};

} // namespace server

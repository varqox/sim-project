#pragma once

#include "string_view.hh"

#if 0
#warning "Before committing disable this debug"
#include "logger.hh"
#define DEBUG_PARSER(...) __VA_ARGS__
#else
#define DEBUG_PARSER(...)
#endif

/**
 * @brief Used to parse Request URI from HTTP requests
 *
 * @details Request has to begin with '/'.
 *   Arguments are delimited by '/' and the last one may be delimited by '?'.
 *   Only after extracting all arguments the query string (delimited by '?')
 *   may be extracted.
 */
class RequestUriParser {
	StringView buff;

public:
	constexpr explicit RequestUriParser(StringView str)
	   : buff(std::move(str)) {}

	constexpr RequestUriParser(const RequestUriParser&) noexcept = default;
	constexpr RequestUriParser(RequestUriParser&&) noexcept = default;
	constexpr RequestUriParser& operator=(const RequestUriParser&) noexcept = default;
	constexpr RequestUriParser& operator=(RequestUriParser&&) noexcept = default;

	/// @brief Extracts next URL argument
	StringView extract_next_arg() {
		if (buff.empty() || buff.front() != '/')
			return {};

		size_t pos = 1;
		while (pos < buff.size() && buff[pos] != '/' && buff[pos] != '?')
			++pos;

		StringView res {buff.substr(1, pos - 1)};
		buff.remove_prefix(pos);
		DEBUG_PARSER(stdlog(__PRETTY_FUNCTION__, " -> extracted: ", res,
		                    " \tleft: ", buff);)
		return res;
	}

	/// Extracts URL Query (iff all arguments were extracted before)
	StringView extract_query() {
		if (buff.empty() || buff.front() != '?')
			return {};

		StringView res {buff.substr(1)};
		buff = "";
		return res;
	}

	StringView& data() noexcept { return buff; }

	constexpr const StringView& data() const noexcept { return buff; }

	constexpr StringView remnant() const noexcept { return buff; }
};

#pragma once

#include "string.h"
#include "logger.h"

#if 0
# warning "Before committing disable this debug"
# define DEBUG_PARSER(...) __VA_ARGS__
#else
# define DEBUG_PARSER(...)
#endif

class SimpleParser {
	StringView buff;

public:
	explicit SimpleParser(const StringView& str) : buff(str) {}

	SimpleParser(const SimpleParser&) noexcept = default;

	SimpleParser(SimpleParser&&) noexcept = default;

	SimpleParser& operator=(const SimpleParser&) noexcept = default;

	SimpleParser& operator=(SimpleParser&&) noexcept = default;

	bool isNext(const StringView& str, char delimiter = '/') const noexcept {
		DEBUG_PARSER(stdlog('\'', buff, "' -> compared with: '", str, "' -> ",
			toString(compareTo(buff, 0, delimiter, str)));)
		return (compareTo(buff, 0, delimiter, str) == 0);
	}

	/**
	 * @brief Extracts next data
	 *
	 * @param isDelimiter functor which should take one argument - char and
	 *   return true for the first character which should not appear in the
	 *   result
	 *
	 * @return Extracted data
	 */
	template<class Func>
	StringView extractNext(Func&& isDelimiter) {
		size_t pos = 0;
		while (pos < buff.size() && !isDelimiter(buff[pos]))
			++pos;

		StringView res{buff.substr(0, pos)};
		buff.removePrefix(pos + 1);
		DEBUG_PARSER(stdlog(__PRETTY_FUNCTION__, " -> extracted: ", res);)
		return res;
	}

	StringView extractNext(char delimiter = '/') noexcept {
		return extractNext([delimiter](char c) { return (c == delimiter); });
	}

	/**
	 * @brief Extracts next data (ignores empty arguments)
	 *
	 * @param isDelimiter functor which should take one argument - char and
	 *   return true for the first character which should not appear in the
	 *   result
	 *
	 * @return Extracted data (empty if there is no more data)
	 */
	template<class Func>
	StringView extractNextNonEmpty(Func&& isDelimiter) {
		buff.removeLeading(isDelimiter); // Ignore delimiters
		return extractNext(std::forward<Func>(isDelimiter));
	}

	StringView extractNextNonEmpty(char delimiter = '/') noexcept {
		return extractNextNonEmpty([delimiter](char c) {
				return (c == delimiter);
			});
	}

	StringView remnant() const noexcept { return buff; }
};

/**
 * @brief Used to parse Request URI from HTTP requests
 *
 * @details Request has to begin with '/'.
 *   Arguments are delimited by '/' and the last one may be delimited by '?'.
 *   Only after extracting all arguments the query string (delimited by '?')
 *   may be extracted.
 */
class RequestURIParser {
	StringView buff;

public:
	explicit RequestURIParser(const StringView& str) : buff(str) {}

	RequestURIParser(const RequestURIParser&) noexcept = default;

	RequestURIParser(RequestURIParser&&) noexcept = default;

	RequestURIParser& operator=(const RequestURIParser&) noexcept = default;

	RequestURIParser& operator=(RequestURIParser&&) noexcept = default;

	~RequestURIParser() = default;

	/// @brief Extracts next URL argument
	StringView extractNextArg() {
		if (buff.empty() || buff.front() != '/')
			return {};

		size_t pos = 1;
		while (pos < buff.size() && buff[pos] != '/' && buff[pos] != '?')
			++pos;

		StringView res {buff.substr(1, pos - 1)};
		buff.removePrefix(pos);
		DEBUG_PARSER(stdlog(__PRETTY_FUNCTION__, " -> extracted: ", res,
			" \tleft: ", buff);)
		return res;
	}

	/// Extracts URL Query (if all arguments were extracted before)
	StringView extractQuery() {
		if (buff.empty() || buff.front() != '?')
			return {};

		StringView res {buff.substr(1)};
		buff.clear();
		return res;
	}

	StringView remnant() const noexcept { return buff; }
};

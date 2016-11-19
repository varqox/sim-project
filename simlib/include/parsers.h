#pragma once

#include "logger.h"
#include "string.h"

#if 0
# warning "Before committing disable this debug"
# define DEBUG_PARSER(...) __VA_ARGS__
#else
# define DEBUG_PARSER(...)
#endif

class SimpleParser : public StringView {
public:
	constexpr explicit SimpleParser(const StringView& s) : StringView(s) {}

	constexpr SimpleParser(const SimpleParser&) noexcept = default;
	constexpr SimpleParser(SimpleParser&&) noexcept = default;
	SimpleParser& operator=(const SimpleParser&) noexcept = default;
	SimpleParser& operator=(SimpleParser&&) noexcept = default;

	bool isNext(const StringView& s, char delimiter = '/') const noexcept {
		DEBUG_PARSER(stdlog('\'', buff, "' -> compared with: '", s, "' -> ",
			toStr(compareTo(buff, 0, delimiter, s)));)
		return (compareTo(*this, 0, delimiter, s) == 0);
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
		while (pos < size() && !isDelimiter((*this)[pos]))
			++pos;

		StringView res {substr(0, pos)};
		removePrefix(pos + 1); // Safe - pos + 1 is cut down to size() if needed
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
		removeLeading(isDelimiter); // Ignore delimiters
		return extractNext(std::forward<Func>(isDelimiter));
	}

	StringView extractNextNonEmpty(char delimiter = '/') noexcept {
		return extractNextNonEmpty([delimiter](char c) {
				return (c == delimiter);
			});
	}
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
	constexpr explicit RequestURIParser(const StringView& str) : buff(str) {}

	constexpr RequestURIParser(const RequestURIParser&) noexcept = default;
	constexpr RequestURIParser(RequestURIParser&&) noexcept = default;
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

	StringView& data() noexcept { return buff; }

	constexpr const StringView& data() const noexcept { return buff; }

	constexpr StringView remnant() const noexcept { return buff; }
};

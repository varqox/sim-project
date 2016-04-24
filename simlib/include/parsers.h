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

	SimpleParser(const SimpleParser& up) : buff(up.buff) {}

	SimpleParser(SimpleParser&& up) : buff(std::move(up.buff)) {}

	SimpleParser& operator=(const SimpleParser& up)  {
		buff = up.buff;
		return *this;
	}

	SimpleParser& operator=(SimpleParser&& up) {
		buff = std::move(up.buff);
		return *this;
	}

	~SimpleParser() = default;

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

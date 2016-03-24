#pragma once

#include "string.h"
#include "logger.h"

#if 0
# warning "Before committing disable this debug"
#define DEBUG_PARSER(...) __VA_ARGS__
#else
#define DEBUG_PARSER(...)
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

	bool isNext(const StringView& str, char delimeter = '/') const noexcept {
		DEBUG_PARSER(stdlog('\'', buff, "' -> compared with: '", str, "' -> ",
			toString(compareTo(buff, 0, delimeter, str)));)
		return (compareTo(buff, 0, delimeter, str) == 0);
	}

	/**
	 * @brief Extracts next argument
	 * @details [long description]
	 *
	 * @param isDelimeter functor which should take one argument - char and
	 *   return true for the first character which should not appear in result
	 *
	 * @return [description]
	 */
	template<class Func>
	StringView extractNext(Func&& isDelimeter) {
		size_t pos = 0;
		while (pos < buff.size() && !isDelimeter(buff[pos]))
			++pos;

		StringView res{buff.substr(0, pos)};
		buff.removePrefix(pos + 1);
		DEBUG_PARSER(stdlog(__PRETTY_FUNCTION__, " -> extracted: ", res);)
		return res;
	}

	StringView extractNext(char delimeter = '/') noexcept {
		return extractNext([delimeter](char c) { return (c == delimeter); });
	}

	StringView remnant() const noexcept { return buff; }
};

#pragma once

#include "simlib/string_compare.hh"

#include <utility>

#if 0
#warning "Before committing disable this debug"
#include "simlib/logger.hh"
#define DEBUG_PARSER(...) __VA_ARGS__
#else
#define DEBUG_PARSER(...)
#endif

class SimpleParser : public StringView {
public:
	constexpr explicit SimpleParser(StringView s)
	: StringView(s) {}

	SimpleParser(const SimpleParser&) noexcept = default;
	SimpleParser(SimpleParser&&) noexcept = default;
	SimpleParser& operator=(const SimpleParser&) noexcept = default;
	SimpleParser& operator=(SimpleParser&&) noexcept = default;

	~SimpleParser() = default;

	[[nodiscard]] bool is_next(const StringView& s,
	                           char delimiter = '/') const noexcept {
		DEBUG_PARSER(stdlog('\'', *this, "' -> compared with: '", s, "' -> ",
		                    compare_to(*this, 0, delimiter, s));)
		return (compare_to(*this, 0, delimiter, s) == 0);
	}

	/**
	 * @brief Extracts next data
	 *
	 * @param is_delimiter functor taking one argument - char and return true
	 *   for the first character that should not appear in the result
	 *
	 * @return Extracted data
	 */
	template <class Func>
	StringView extract_next(Func&& is_delimiter) {
		size_t pos = 0;
		while (pos < size() and not is_delimiter((*this)[pos])) {
			++pos;
		}

		StringView res{substr(0, pos)};
		remove_prefix(
		   pos + 1); // Safe because pos + 1 is cut down to size() if needed
		DEBUG_PARSER(stdlog(__PRETTY_FUNCTION__, " -> extracted: ", res);)
		return res;
	}

	StringView extract_next(char delimiter = '/') noexcept {
		return extract_next([delimiter](char c) { return (c == delimiter); });
	}

	/**
	 * @brief Extracts next data (ignores empty arguments)
	 *
	 * @param is_delimiter functor taking one argument - char and return true
	 *   for the first character that should not appear in the result
	 *
	 * @return Extracted data (empty if there is no more data)
	 */
	template <class Func>
	StringView extract_next_non_empty(Func&& is_delimiter) {
		remove_leading(is_delimiter); // Ignore delimiters
		return extract_next(std::forward<Func>(is_delimiter));
	}

	/**
	 * @brief Extracts next data (ignores empty arguments)
	 *
	 * @param delimiter first character that should not appear in the result
	 *
	 * @return Extracted data (empty if there is no more data)
	 */
	StringView extract_next_non_empty(char delimiter = '/') noexcept {
		return extract_next_non_empty(
		   [delimiter](char c) { return (c == delimiter); });
	}
};

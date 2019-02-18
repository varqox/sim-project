#pragma once

#include "string.h"

#include <map>
#include <vector>

class ConfigFile {
public:
	class ParseError : public std::runtime_error {
		std::string diagnostics_;

	public:
		explicit ParseError(const std::string& msg) : runtime_error(msg) {}

		template<class... Args>
		ParseError(size_t line, size_t pos, Args&&... msg)
			: runtime_error(concat_tostr("line ", line, ':', pos, ": ",
				std::forward<Args>(msg)...))
		{}

		ParseError(const ParseError& pe) = default;
		ParseError(ParseError&&) /*noexcept*/ = default;
		ParseError& operator=(const ParseError& pe) = default;
		ParseError& operator=(ParseError&&) /*noexcept*/ = default;

		using runtime_error::what;

		const std::string& diagnostics() const noexcept { return diagnostics_; }

		virtual ~ParseError() noexcept {}

		friend class ConfigFile;
	};

	struct Variable {
		static constexpr uint8_t SET = 1; // set if variable appears in the
		                                  // config
		static constexpr uint8_t ARRAY = 2; // set if variable is an array

		uint8_t flag;
		std::string s;
		std::vector<std::string> a;

		struct ValueSpan {
			size_t beg = 0;
			size_t end = 0;
		} value_span; // value occupies [beg, end)

		Variable() : flag(0) {}

		void unset() noexcept {
			flag = 0;
			s.clear();
			a.clear();
		}

		bool is_set() const noexcept { return flag & SET; }

		bool is_array() const noexcept { return flag & ARRAY; }

		// Returns value as bool or false on error
		bool as_bool() const noexcept {
			return (s == "1" || lower_equal(s, "on") || lower_equal(s, "true"));
		}

		// Returns value as Integer (unsigned only) or 0 on error. To
		// distinguish error from success when return value is 0, set errno to 0
		// before calling this function. If the errno != 0 after the call, then
		// an error was encountered.
		template<class Type>
		std::enable_if_t<std::is_unsigned<Type>::value, Type> as_int() const
			noexcept
		{
			Type x{};
			if (strtou<Type>(s, x) != (int)s.size()) {
				errno = EINVAL;
				return 0; // TODO: maybe throw
			}
			return x;
		};

		// Returns value as Integer or 0 on error. To distinguish error from
		// success when return value is 0, set errno to 0 before calling this
		// function. If the errno != 0 after the call, then an error was
		// encountered.
		template<class Type = int32_t>
		std::enable_if_t<!std::is_unsigned<Type>::value, Type> as_int() const
			noexcept
		{
			Type x{};
			if (strtoi<Type>(s, x) != (int)s.size()) {
				errno = EINVAL;
				return 0; // TODO: maybe throw
			}
			return x;
		};

		// Returns value as double or 0 on error, uses strtod(3)
		double as_double() const noexcept {
			return (is_set() ? strtod(s.c_str(), nullptr) : 0);
		}

		// Returns value as string (empty if not a string or variable isn't set)
		const std::string& as_string() const noexcept { return s; }

		// Returns value as array (empty if not an array or variable isn't set)
		const std::vector<std::string>& as_array() const noexcept { return a; }
	};

private:
	std::map<std::string, Variable, std::less<>> vars; // (name => value)
	// TODO: ^ maybe StringView would be better?
	static const Variable null_var;

public:
	ConfigFile() = default;

	ConfigFile(const ConfigFile&) = default;
	ConfigFile(ConfigFile&&) noexcept = default;
	ConfigFile& operator=(const ConfigFile&) = default;
	ConfigFile& operator=(ConfigFile&&) noexcept = default;

	~ConfigFile() {}

	// Adds variables @p names to variable set, ignores duplications
	template<class... Args>
	void add_vars(Args&&... names) {
		int t[] = {(vars.emplace(std::forward<Args>(names), Variable{}), 0)...};
		(void)t;
	}

	// Clears variable set
	void clear() { vars.clear(); }

	// Returns a reference to a variable @p name from variable set or to a
	// null_var
	const Variable& get_var(StringView name) const noexcept {
		return (*this)[name];
	}

	const Variable& operator[](StringView name) const noexcept {
		auto it = vars.find(name);
		return (it != vars.end() ? it->second : null_var);
	}

	const decltype(vars)& get_vars() const { return vars; }

	 /**
	 * @brief Loads config (variables) form file @p pathname
	 * @details Uses load_config_from_string()
	 *
	 * @param pathname config file
	 * @param load_all whether load all variables from @p pathname or load only
	 *   these from variable set
	 *
	 * @errors Throws an exception std::runtime_error if an open(2) error occurs
	 *   and all exceptions from load_config_from_string()
	 */
	void load_config_from_file(FilePath pathname, bool load_all = false);

	/**
	 * @brief Loads config (variables) form string @p config
	 *
	 * @param config input string
	 * @param load_all whether load all variables from @p config or load only
	 *   these from variable set
	 *
	 * @errors Throws an exception (ParseError) if an error occurs
	 */
	void load_config_from_string(std::string config, bool load_all = false);

	// Check if string @p str is a valid string literal
	static bool is_string_literal(StringView str);

	/**
	 * @brief Escapes unsafe sequences in str
	 * @details '\'' replaces with "''"
	 *
	 * @param str input string
	 *
	 * @return escaped, single-quoted string
	 */
	static std::string escape_to_single_quoted_string(StringView str);

	/**
	 * @brief Escapes unsafe sequences in str
	 * @details Escapes '\"' with "\\\"" and characters for which
	 *   iscntrl(3) != 0 with "\\xnn" sequence where n is a hexadecimal digit.
	 *
	 * @param str input string
	 *
	 * @return escaped, double-quoted string
	 */
	static std::string escape_to_double_quoted_string(StringView str);

	/**
	 * @brief Escapes unsafe sequences in str
	 * @details Escapes '\"' with "\\\"" and characters for which
	 *   isprint(3) == 0 with "\\xnn" sequence where n is a hexadecimal digit.
	 *   The difference between one-argument version is that characters (bytes)
	 *   not from interval [0, 127] are also escaped hexadecimally, that is why
	 *   this option is not recommended when using utf-8 encoding - it will
	 *   escape every non-ascii character (byte)
	 *
	 * @param str input string
	 * @param int - it is totally unused (used only to distinguish functions)
	 *
	 * @return escaped, double-quoted string
	 */
	static std::string escape_to_double_quoted_string(StringView str, int);

	/**
	 * @brief Converts string @p str so that it can be safely placed in config
	 *   file
	 * @details Possible cases:
	 *   1) @p str contains '\'' or character for which iscrtnl(3) != 0:
	 *     Escaped @p str via one-argument escape_to_double_quoted_string() will be
	 *     returned.
	 *   2) Otherwise if @p str is string literal (is_string_literal(@p str)):
	 *     Unchanged @p str will be returned.
	 *   3) Otherwise:
	 *     Escaped @p str via escape_to_single_quoted_string() will be returned.
	 *
	 *   Examples:
	 *     "" -> '' (single quoted string, special case)
	 *     "foo-bar" -> foo-bar (string literal)
	 *     "line: 1\nab d E\n" -> "line: 1\nab d E\n" (double quoted string)
	 *     " My awesome text" -> ' My awesome text' (single quoted string)
	 *     " \\\\\\\\ " -> ' \\\\ ' (single quoted string)
	 *     " '\t\n' ś " -> " '\t\n' ś " (double quoted string)
	 *
	 * @param str input string
	 *
	 * @return escaped (and possibly quoted) string
	 */
	static std::string escape_string(StringView str);

	/**
	 * @brief Converts string @p str so that it can be safely placed in config
	 *   file
	 * @details Possible cases:
	 *   1) @p str contains '\'' or character for which isprint(3) == 0:
	 *     Escaped @p str via two-argument escape_to_double_quoted_string() will be
	 *     returned.
	 *   2) Otherwise if @p str is string literal (is_string_literal(@p str)):
	 *     Unchanged @p str will be returned.
	 *   3) Otherwise:
	 *     Escaped @p str via escape_to_single_quoted_string() will be returned.
	 *
	 *   The difference between one-argument version is that in this version
	 *   characters not from interval [0, 127] are also escaped hexadecimally
	 *   with two-argument escape_to_double_quoted_string()
	 *
	 *   Examples:
	 *     "" -> '' (single quoted string, special case)
	 *     "foo-bar" -> foo-bar (string literal)
	 *     "line: 1\nab d E\n" -> "line: 1\nab d E\n" (double quoted string)
	 *     " My awesome text" -> ' My awesome text' (single quoted string)
	 *     " \\\\\\\\ " -> ' \\\\ ' (single quoted string)
	 *     " '\t\n' ś " -> " '\t\n' ś " (double quoted string)
	 *
	 * @param str input string
	 *
	 * @return escaped (and possibly quoted) string
	 */
	static std::string escape_string(StringView str, int);
};

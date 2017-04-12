#pragma once

#include "string.h"

#include <map>
#include <vector>

class ConfigFile {
public:
	class ParseError : public std::runtime_error {
	public:
		explicit ParseError(const std::string& msg) : runtime_error(msg) {}

		template<class... Args>
		ParseError(size_t line, size_t pos, Args&&... msg)
			: runtime_error(concat("line ", toStr(line), ':', toStr(pos), ": ",
				msg...)) {}

		ParseError(const ParseError& pe) : runtime_error(pe) {}

		using runtime_error::operator=;
		using runtime_error::what;

		virtual ~ParseError() noexcept {}
	};

	struct Variable {
		static constexpr uint8_t SET = 1; // set if variable appears in the
		                                  // config
		static constexpr uint8_t ARRAY = 2; // set if variable is an array

		uint8_t flag;
		std::string s;
		std::vector<std::string> a;

		Variable() : flag(0) {}

		void unset() noexcept {
			flag = 0;
			s.clear();
			a.clear();
		}

		bool isSet() const noexcept { return flag & SET; }

		bool isArray() const noexcept { return flag & ARRAY; }

		// Returns value as bool or false on error
		bool asBool() const noexcept {
			return (s == "1" || lower_equal(s, "on") || lower_equal(s, "true"));
		}

		// Returns value as Integer (unsigned only) or 0 on error. To
		// distinguish error from success when return value is 0, set errno to 0
		// before calling this function. If the errno != 0 after the call, then
		// an error was encountered.
		template<class Type>
		typename std::enable_if<std::is_unsigned<Type>::value, Type>::type
			asInt() const noexcept
		{
			Type x{};
			if (strtou<Type>(s, &x) != (int)s.size()) {
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
		typename std::enable_if<!std::is_unsigned<Type>::value, Type>::type
			asInt() const noexcept
		{
			Type x{};
			if (strtoi<Type>(s, &x) != (int)s.size()) {
				errno = EINVAL;
				return 0; // TODO: maybe throw
			}
			return x;
		};

		// Returns value as double or 0 on error, uses strtod(3)
		double asDouble() const noexcept {
			return (isSet() ? strtod(s.c_str(), nullptr) : 0);
		}

		// Returns value as string (empty if not a string or variable isn't set)
		const std::string& asString() const noexcept { return s; }

		// Returns value as array (empty if not an array or variable isn't set)
		const std::vector<std::string>& asArray() const noexcept { return a; }
	};

private:
	std::map<std::string, Variable> vars; // (name => value)
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
	void addVars(Args&&... names) {
		int t[] = {(vars.emplace(std::forward<Args>(names), Variable{}), 0)...};
		(void)t;
	}

	// Clears variable set
	void clear() { vars.clear(); }

	// Returns a reference to a variable @p name from variable set or to a
	// null_var
	const Variable& getVar(const std::string& name) const noexcept {
		// TODO: when map will be replaced with sth better use StringView as arg
		return (*this)[name];
	}

	const Variable& operator[](const std::string& name) const noexcept {
		// TODO: when map will be replaced with sth better use StringView as arg
		auto it = vars.find(name);
		return (it != vars.end() ? it->second : null_var);
	}

	const std::map<std::string, Variable>& getVars() const { return vars; }

	 /**
	 * @brief Loads config (variables) form file @p pathname
	 * @details Uses loadConfigFromString()
	 *
	 * @param pathname config file
	 * @param load_all whether load all variables from @p pathname or load only
	 *   these from variable set
	 *
	 * @errors Throws an exception std::runtime_error if an open(2) error occurs
	 *   and all exceptions from loadConfigFromString()
	 */
	void loadConfigFromFile(const CStringView& pathname, bool load_all = false);

	/**
	 * @brief Loads config (variables) form string @p config
	 *
	 * @param config input string
	 * @param load_all whether load all variables from @p config or load only
	 *   these from variable set
	 *
	 * @errors Throws an exception (ParseError) if an error occurs
	 */
	void loadConfigFromString(std::string config, bool load_all = false);

	// Check if string @p str is a valid string literal
	static bool isStringLiteral(const StringView& str);

	/**
	 * @brief Escapes unsafe sequences in str
	 * @details '\'' replaces with "''"
	 *
	 * @param str input string
	 *
	 * @return escaped, single-quoted string
	 */
	static std::string escapeToSingleQuotedString(const StringView& str);

	/**
	 * @brief Escapes unsafe sequences in str
	 * @details Escapes '\"' with "\\\"" and characters for which
	 *   iscntrl(3) != 0 with "\\xnn" sequence where n is a hexadecimal digit.
	 *
	 * @param str input string
	 *
	 * @return escaped, double-quoted string
	 */
	static std::string escapeToDoubleQuotedString(const StringView& str);

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
	static std::string escapeToDoubleQuotedString(const StringView& str, int);

	/**
	 * @brief Converts string @p str so that it can be safely placed in config
	 *   file
	 * @details Possible cases:
	 *   1) @p str contains '\'' or character for which iscrtnl(3) != 0:
	 *     Escaped @p str via one-argument escapeToDoubleQuotedString() will be
	 *     returned.
	 *   2) Otherwise if @p str is string literal (isStringLiteral(@p str)):
	 *     Unchanged @p str will be returned.
	 *   3) Otherwise:
	 *     Escaped @p str via escapeToSingleQuotedString() will be returned.
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
	static std::string escapeString(const StringView& str);

	/**
	 * @brief Converts string @p str so that it can be safely placed in config
	 *   file
	 * @details Possible cases:
	 *   1) @p str contains '\'' or character for which isprint(3) == 0:
	 *     Escaped @p str via two-argument escapeToDoubleQuotedString() will be
	 *     returned.
	 *   2) Otherwise if @p str is string literal (isStringLiteral(@p str)):
	 *     Unchanged @p str will be returned.
	 *   3) Otherwise:
	 *     Escaped @p str via escapeToSingleQuotedString() will be returned.
	 *
	 *   The difference between one-argument version is that in this version
	 *   characters not from interval [0, 127] are also escaped hexadecimally
	 *   with two-argument escapeToDoubleQuotedString()
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
	static std::string escapeString(const StringView& str, int);
};

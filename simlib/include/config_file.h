#pragma once

#include "string.h"

#include <map>
#include <vector>

class ConfigFile {
	/**
	 * @brief Extracts single quoted string from @p in
	 *
	 * @param in input string
	 * @param out output string, should be empty (function will append to it)
	 * @param line config file line number
	 *
	 * @return Number of @p in characters parsed
	 */
	static size_t parseSingleQuotedString(const StringView& in,
		std::string& out, size_t line);

	/**
	 * @brief Extracts double quoted string from @p in
	 *
	 * @param in input string
	 * @param out output string, should be empty (function will append to it)
	 * @param line config file line number
	 *
	 * @return Number of @p in characters parsed
	 */
	static size_t parseDoubleQuotedString(const StringView& in,
		std::string& out, size_t line);

	/**
	 * @brief Extracts value from @p in
	 *
	 * @param in not empty input string
	 * @param out output string, should be empty (function will append to it)
	 * @param line config file line number
	 *
	 * @return Number of @p in characters parsed
	 */
	static size_t extractValue(const StringView& in, std::string& out,
		size_t line);

public:
	class ParseError : public std::runtime_error {
	public:
		explicit ParseError(const std::string& msg) : runtime_error(msg) {}

		explicit ParseError(size_t line, const std::string& msg)
			: runtime_error(concat("line ", toString(line), ": ", msg)) {}

		ParseError(const ParseError& pe) : runtime_error(pe) {}

		using runtime_error::operator=;

		using runtime_error::what;

		virtual ~ParseError() noexcept {}
	};

	struct Variable {
		static constexpr int SET = 1; // set if variable appear in config
		static constexpr int ARRAY = 2; // set if variable is array
		uint8_t flag;
		std::string s;
		std::vector<std::string> a;

		Variable() : flag(0) {}

		bool isSet() const { return flag & SET; }

		bool isArray() const { return flag & ARRAY; }
	};

private:
	std::map<std::string, Variable> vars; // (name => value)

public:
	ConfigFile() = default;

	ConfigFile(const ConfigFile& cf) : vars(cf.vars) {}

	ConfigFile(ConfigFile&& cf) : vars(std::move(cf.vars)) {}

	ConfigFile& operator=(const ConfigFile& cf) {
		vars = cf.vars;
		return *this;
	}

	ConfigFile& operator=(ConfigFile&& cf) {
		vars = std::move(cf.vars);
		return *this;
	}

	~ConfigFile() {}

	 /**
	 * @brief Loads config (variables) form file @p pathname
	 * @details Uses loadConfigFromString()
	 *
	 * @param pathname config file
	 * @param load_all whether load all variables from @p pathname or load only
	 *   these from variable set
	 *
	 * @errors Throws an exception std::runtime_error if an open() error occurs
	 *   and all exceptions from loadConfigFromString()
	 */
	void loadConfigFromFile(const std::string& pathname, bool load_all = false);

	/**
	 * @brief Loads config (variables) form string @p config
	 *
	 * @param config input string
	 * @param load_all whether load all variables from @p config or load only
	 *   these from variable set
	 *
	 * @errors Throws an exception (ParseError) if an error occurs
	 */
	void loadConfigFromString(const StringView& config, bool load_all = false);

	// Clears variable set
	void clear() { vars.clear(); }

	// Adds variable @p name to variable set or throw an exception if variable
	// with the same name already exists
	void addVar(const std::string& name) {
		if (!vars.insert(std::make_pair(name, Variable())).second)
			throw std::logic_error(concat("Variable '", name, "' already set"));
	}

	const std::map<std::string, Variable>& getVars() const { return vars; }

	// Checks whether variable @p name exist
	bool exist(const std::string& name) const {
		return vars.find(name) != vars.end();
	}

	// Checks whether variable @p name is set
	bool isSet(const std::string& name) const {
		std::map<std::string, Variable>::const_iterator it = vars.find(name);
		return it != vars.end() && it->second.isSet();
	}

	// Checks whether variable @p name is an array
	bool isArray(const std::string& name) const {
		std::map<std::string, Variable>::const_iterator it = vars.find(name);
		return it != vars.end() && it->second.isArray();
	}

	// Returns variable @p name as int or 0 on error
	int getInt(const StringView& name) const;

	// Returns variable @p name as bool or false on error
	bool getBool(const StringView& name) const;

	// Returns variable @p name as double or 0 on error, uses strtod(3)
	double getReal(const StringView& name) const;

	// Returns variable @p name as string or "" on error
	std::string getString(const StringView& name) const;

	// Returns variable @p name as array (empty on error)
	std::vector<std::string> getArray(const StringView& name) const;

	// Check whether character is one of these [a-zA-Z0-9\-_.]
	static bool isName(int c) {
		return isalnum(c) || c == '-' || c == '_' || c == '.';
	}

	// Check whether character is one of these [a-zA-Z0-9\-_.+:\*]
	static bool isStringLiteral(int c) {
		return isName(c) || c == '+' || c == ':' || c == '*';
	}

	// Check whether string is a valid string literal
	static bool isStringLiteral(const StringView& str);

	/**
	 * @brief Escapes unsafe sequences in str
	 * @details '\'' replaces with "''"
	 *
	 * @param str input string
	 *
	 * @return escaped string
	 */
	static std::string safeSingleQuotedString(const StringView& str);

	/**
	 * @brief Escapes unsafe sequences in str
	 * @details '\'' replaces with "''"
	 *
	 * @param str input string
	 * @param escape_unprintable whether escape unprintable characters via \xnn
	 *
	 * @return escaped string
	 */
	static std::string safeDoubleQuotedString(const StringView& str,
		bool escape_unprintable = false);
};

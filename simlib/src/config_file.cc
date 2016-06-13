#include "../include/config_file.h"
#include "../include/debug.h"
#include "../include/filesystem.h"
#include "../include/logger.h"

#include <cassert>

using std::map;
using std::string;
using std::vector;

#if 0
# warning "Before committing disable this debug"
# define DEBUG_CONFIG_FILE(...) stdlog(__VA_ARGS__)
#else
# define DEBUG_CONFIG_FILE(...)
#endif

/**
 * @brief Extracts single quoted string from @p in
 *
 * @param in input string (cannot contain '\n' and cannot begin with
 *   white space)
 * @param out output string, should be empty (function will append to it)
 * @param line config file line number
 *
 * @return Number of @p in characters parsed
 */
static size_t parseSingleQuotedString(const StringView& in, string& out,
	size_t line)
{
	for (size_t i = 1, end = in.size();; ++i) {
		if (i == end)
			throw ConfigFile::ParseError(line,
				"missing terminating ' character");
		// Escape double single quote or end string
		if (in[i] == '\'') {
			++i;
			if (i == end || in[i] != '\'') // String end
				return i;
		}
		out += in[i];
	}
}


/**
 * @brief Extracts double quoted string from @p in
 *
 * @param in input string (cannot contain '\n' and cannot begin with
 *   white space)
 * @param out output string, should be empty (function will append to it)
 * @param line config file line number
 *
 * @return Number of @p in characters parsed
 */
static size_t parseDoubleQuotedString(const StringView& in, string& out,
	size_t line)
{
	for (size_t i = 1, end = in.size();; ++i) {
		if (i == end)
			throw ConfigFile::ParseError(line,
				"missing terminating \" character");

		if (in[i] == '"') // String end
			return i + 1;

		if (in[i] == '\\') {
			if (++i == end)
				throw ConfigFile::ParseError(line,
					"multi-line strings are not supported");

			switch (in[i]) {
			case '\'':
				out += '\'';
				continue;
			case '\"':
				out += '\"';
				continue;
			case '?':
				out += '?';
				continue;
			case '\\':
				out += '\\';
				continue;
			case 'a':
				out += '\a';
				continue;
			case 'b':
				out += '\b';
				continue;
			case 'f':
				out += '\f';
				continue;
			case 'n':
				out += '\n';
				continue;
			case 'r':
				out += '\r';
				continue;
			case 't':
				out += '\t';
				continue;
			case 'v':
				out += '\v';
				continue;
			case 'x':
				if (i + 2 >= end)
					throw ConfigFile::ParseError(line,
						"incomplete escape sequence \\x");
				++i;
				out += static_cast<char>(hextodec(in[i]) * 16 +
					hextodec(in[i + 1]));
				++i;
				continue;
			default:
				throw ConfigFile::ParseError(line,
					concat("unknown escape sequence \\", in[i]));
			}
			// Should never reach here
		}
		out += in[i];
	}
}


/**
 * @brief Extracts value from @p in
 *
 * @param in not empty input string (cannot contain '\n' and cannot begin with
 *   white space)
 * @param out output string, should be empty (function will append to it)
 * @param line config file line number
 *
 * @return Number of @p in characters parsed
 */
static size_t extractValue(const StringView& in, string& out,
	size_t line)
{
	// Single quoted string
	if (in.front() == '\'')
		return parseSingleQuotedString(in, out, line);

	// Double quoted string
	if (in.front() == '"')
		return parseDoubleQuotedString(in, out, line);

	/* String literal (not array) */
	size_t i = 0, end = in.size();
	while (i < end && in[i] != ']' && in[i] != ',' &&
		(in[i] != '#' || (i && !isspace(in[i - 1]))))
	{
		++i;
	}
	// Remove trailing white space
	while (i && isspace(in[i - 1]))
		--i;
	out.assign(in.data(), i);

	return i;
}

void ConfigFile::loadConfigFromFile(const string& pathname, bool load_all) {
	int fd;
	while ((fd = open(pathname.c_str(), O_RDONLY | O_LARGEFILE)) == -1 &&
		errno == EINTR) {}

	if (fd == -1)
		THROW("Failed to open '", pathname, '\'', error(errno));

	Closer closer(fd);
	string contents = getFileContents(fd);

	loadConfigFromString(contents, load_all);
}

void ConfigFile::loadConfigFromString(const StringView& config, bool load_all) {
	// Set all variables as unused
	for (auto& i : vars)
		i.second.flag = 0;

	Variable tmp; // Used for ignored variables
	size_t beg, end = -1, x, line = 0;
	auto ignoreWhitespace = [&] {
		while (beg < end && isspace(config[beg]))
			++beg;
	};

	// Checks whether character is one of these [a-zA-Z0-9\-_.]
	auto isName = [](int c) -> bool {
		return isalnum(c) || c == '-' || c == '_' || c == '.';
	};

	DEBUG_CONFIG_FILE("Beginning of config file.\n", config);
	while ((beg = ++end) < config.size()) {
		end = std::min(config.find('\n', beg), config.size());
		++line;
		tmp.flag = 0;
		ignoreWhitespace();
		assert(beg <= end);
		if (beg == end || config[beg] == '#') // Blank line or comment
			continue;


		/* Extract name */
		x = beg;
		while (beg < end && isName(config[beg]))
			++beg;
		string name(config.data() + x, config.data() + beg);
		if (name.empty())
			throw ParseError(line, "missing variable name");

		DEBUG_CONFIG_FILE("Variable: '" , name, '\'');

		// Get corresponding variable
		auto it = (load_all ? vars.insert(make_pair(name, Variable())).first
			: vars.find(name));

		Variable& var = (it == vars.end() ? tmp : it->second);
		if (var.flag & Variable::SET)
			throw ParseError(line, concat("variable '", name,
				"' defined more than once"));

		var.flag |= Variable::SET;
		var.s.clear();
		var.a.clear();


		/* Assignment operator */
		ignoreWhitespace();
		// Assert beg < end
		if (beg == end)
			throw ParseError(line, "missing assignment operator");

		if (config[beg] != ':' && config[beg] != '=')
			throw ParseError(line, concat("wrong assignment operator '",
				config[beg], '\''));

		++beg; // Ignore assignment operator
		ignoreWhitespace();
		assert(beg <= end);
		if (beg == end || config[beg] == '#') // No value or comment
			continue;

		/* Extract value */
		if (config[beg] != '[') { // Value
			beg += extractValue(config.substr(beg, end - beg), var.s, line);
			DEBUG_CONFIG_FILE("\t -> value: '", var.s, '\'');

		} else { // Array
			++beg; // Ignore '['
			var.flag |= Variable::ARRAY;
			DEBUG_CONFIG_FILE("\tARRAY:");

			bool expect_value = true;
			for (;;) {
				ignoreWhitespace();
				// Config end
				if (beg >= config.size())
					throw ParseError(line, "incomplete array");

				// Next line
				if (beg == end || config[beg] == '#') {
					beg = ++end;
					end = std::min(config.find('\n', beg), config.size());
					++line;
					continue;
				}

				// Value
				if (expect_value) {
					// Empty array
					if (var.a.empty() && config[beg] == ']') {
						++beg;
						break;
					}

					var.a.emplace_back();
					beg += extractValue(config.substr(beg, end - beg),
						var.a.back(), line);
					expect_value = false;

					DEBUG_CONFIG_FILE("\t -> ", toString(var.a.size() - 1),
						": '", var.a.back(), '\'');

				// Comma
				} else if (config[beg] == ',') {
					++beg;
					expect_value = true;

				// End of array
				} else if (config[beg] == ']') {
					++beg;
					break;
				} else
					throw ParseError(line, "Expected ',' or ']' after value");
			}
		}

		ignoreWhitespace();
		if (beg < end && config[beg] != '#') // not comment
			throw ParseError(line, concat("Unknown character at the end of "
				"line: ", config[beg]));
	}
	DEBUG_CONFIG_FILE("End of config file.");
}

int ConfigFile::getInt(const StringView &name) const {
	auto it = vars.find(name.to_string());
	if (it == vars.end())
		return 0;

	int x;
	return strtoi(it->second.s, &x) > 0 ? x : 0;
}

bool ConfigFile::getBool(const StringView &name) const {
	auto it = vars.find(name.to_string());
	if (it == vars.end())

		return false;

	string val = tolower(it->second.s);
	return (val == "1" || val == "on" || val == "true");
}

double ConfigFile::getReal(const StringView &name) const {
	auto it = vars.find(name.to_string());
	return (it == vars.end() ? 0 : strtod(it->second.s.c_str(), nullptr));
}

string ConfigFile::getString(const StringView &name) const {
	auto it = vars.find(name.to_string());
	return (it == vars.end() ? "" : it->second.s);
}

vector<string> ConfigFile::getArray(const StringView& name) const {
	auto it = vars.find(name.to_string());
	return (it == vars.end() ? vector<string>{} : it->second.a);
}

bool ConfigFile::isStringLiteral(const StringView& str) {
	if (str.empty())
		return true;

	// Special check on the first and last character
	if (str[0] == '[' || str[0] == '\'' || str[0] == '"' || str[0] == '#' ||
		isspace(str[0]) || isspace(str.back()))
	{
		return false;
	}

	for (size_t i = 0; i < str.size(); ++i)
		if (str[i] == '\n' || str[i] == ']' || str[i] == ',' ||
			(str[i] == '#' && isspace(str[i - 1])))
		{ // according to above check     ^ this is safe
			return false;
		}

	return true;
}

string ConfigFile::safeSingleQuotedString(const StringView& str) {
	string res;
	res.reserve(str.size());
	for (size_t i = 0; i < str.size(); ++i) {
		res += str[i];
		if (str[i] == '\'')
			res += '\'';
	}
	return res;
}

string ConfigFile::safeDoubleQuotedString(const StringView& str,
	bool escape_unprintable)
{
	string res;
	res.reserve(str.size());
	for (size_t i = 0; i < str.size(); ++i)
		switch (str[i]) {
		case '\\':
			res += "\\\\";
			break;
		case '\0':
			res += "\\0";
			break;
		case '"':
			res += "\\\"";
			break;
		case '?':
			res += "\\?";
			break;
		case '\a':
			res += "\\a";
			break;
		case '\b':
			res += "\\b";
			break;
		case '\f':
			res += "\\f";
			break;
		case '\n':
			res += "\\n";
			break;
		case '\r':
			res += "\\r";
			break;
		case '\t':
			res += "\\t";
			break;
		case '\v':
			res += "\\v";
			break;
		default:
			if (escape_unprintable && !isprint(str[i])) {
				res += "\\x";
				res += dectohex(static_cast<unsigned char>(str[i]) >> 4);
				res += dectohex(static_cast<unsigned char>(str[i]) & 15);
			} else
				res += str[i];
		}

	return res;
}

string ConfigFile::safeString(const StringView& str, bool escape_unprintable) {
	if (isStringLiteral(str))
		return str.to_string();

	if (escape_unprintable) {
		for (size_t i = 0; i < str.size(); ++i)
			if (!isprint(str[i]))
				return concat('"', safeDoubleQuotedString(str, true), '"');

	} else if (str.find('\n') != StringView::npos)
		return concat('"', safeDoubleQuotedString(str, false), '"');

	return concat('\'', safeSingleQuotedString(str), '\'');
}

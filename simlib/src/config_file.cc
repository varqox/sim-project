#include "../include/config_file.h"
#include "../include/debug.h"
#include "../include/filesystem.h"
#include "../include/parsers.h"
#include "../include/utilities.h"

using std::map;
using std::string;
using std::vector;

#if 0
# warning "Before committing disable this debug"
# define DEBUG_CF(...) __VA_ARGS__
#else
# define DEBUG_CF(...)
#endif

const ConfigFile::Variable ConfigFile::null_var;

void ConfigFile::loadConfigFromFile(const string& pathname, bool load_all) {
	string contents = getFileContents(pathname);
	loadConfigFromString(contents, load_all);
}

void ConfigFile::loadConfigFromString(string config, bool load_all) {
	// Set all variables as unused
	for (auto& i : vars)
		i.second.unset();

	// Checks whether c is a white-space but not a newline
	auto isWs = [](int c) { return (c != '\n' && isspace(c)); };
	// Checks whether character is one of these [a-zA-Z0-9\-_.]
	auto isName = [](int c) {
		return (isalnum(c) || c == '-' || c == '_' || c == '.');
	};

	DEBUG_CF(stdlog("Config file:\n", config);)

	config += '\n'; // Now each line ends with a newline character
	SimpleParser buff {config};

	Variable tmp; // Used for ignored variables
	auto buff_beg = buff.data();
	#if __cplusplus > 201103L
	# warning "Use variadic generic lambda instead"
	#endif

	#define throw_parse_error(...) { \
			string::size_type pos = buff.data() - buff_beg - buff.empty(); \
			string::size_type x = pos; /* Position of the last newline before
				pos */ \
			while (x && config[--x] != '\n') {} \
			\
			int line = 1 + std::count(buff_beg, buff_beg + x + 1, '\n'); \
			\
			ParseError pe(line, pos - x + (line == 1), __VA_ARGS__); \
			DEBUG_CF(stdlog("Throwing exception: ", pe.what());) \
			throw pe; \
		}

	auto extract_value = [&] {
		string res;
		size_t i = 0;
		// Single-quoted string
		if (buff[0] == '\'') {
			while (buff[++i] != '\n') {
				if (buff[i] == '\'') {
					if (buff[i + 1] != '\'') { // Safe (newline is at the end of
					                           // every line)
						buff.removePrefix(i + 1);
						return res;
					}

					++i;
				}
				res += buff[i];
			}
			buff.removePrefix(i);
			throw_parse_error("Missing terminating ' character");

		}
		// Double-quoted string
		if (buff[0] == '"') {
			while (buff[++i] != '\n') {
				if (buff[i] == '"') {
					buff.removePrefix(i + 1);
					return res;
				}

				if (buff[i] != '\\') {
					res += buff[i];
					continue;
				}

				// Escape sequence
				switch (buff[++i]){
				case '\'': res += '\''; continue;
				case '"': res += '"'; continue;
				case '?': res += '?'; continue;
				case '\\': res += '\\'; continue;
				case 't': res += '\t'; continue;
				case 'a': res += '\a'; continue;
				case 'b': res += '\b'; continue;
				case 'f': res += '\f'; continue;
				case 'n': res += '\n'; continue;
				case 'r': res += '\r'; continue;
				case 'v': res += '\v'; continue;
				case 'x':
					// i will not go out of the buffer - (guard = newline)
					if (!isxdigit(buff[++i])) {
						buff.removePrefix(i);
						throw_parse_error("Invalid hexadecimal digit: `",
							buff[0], '`');
					}
					if (!isxdigit(buff[++i])) {
						buff.removePrefix(i);
						throw_parse_error("Invalid hexadecimal digit: `",
							buff[0], '`');
					}
					res += static_cast<char>((hextodec(buff[i - 1]) << 4) +
						hextodec(buff[i]));
					continue;
				default:
					buff.removePrefix(i);
					throw_parse_error("Unknown escape sequence: `\\", buff[0],
						'`');
				}

			}
			buff.removePrefix(i);
			throw_parse_error("Missing terminating \" character");

		}

		// String literal

		// Check if the string literal begins
		if (buff[0] == '[' || buff[0] == ',' || buff[0] == ']')
			throw_parse_error("Invalid beginning of the string literal: `",
				buff[0], '`');

		// Beginning is ok
		// Interior
		while (buff[i + 1] != '\n' && buff[i + 1] != '#' &&
			buff[i + 1] != ']' && buff[i + 1] != ',')
		{
			++i;
		}
		// Remove white-spaces from ending
		while (isspace(buff[i]))
			--i;
		res = buff.substring(0, ++i).to_string();

		buff.removePrefix(i);
		return res;
	};

	while (buff.size()) {
		auto skip_comment = [&] {
			buff.removeLeading([](char c) { return (c != '\n'); });
		};

		buff.removeLeading(isWs);
		// Newline
		if (buff[0] == '\n') {
			buff.removePrefix(1);
			continue;
		}
		// Comment
		if (buff[0] == '#') {
			skip_comment();
			buff.removePrefix(1);
			continue;
		}

		/* Variable name */
		StringView name = buff.extractLeading(isName);
		if (name.empty())
			throw_parse_error("Invalid or missing variable name");

		DEBUG_CF(stdlog("Variable: ", name);)

		/* Assignment operator */
		buff.removeLeading(isWs);
		if (buff[0] == '\n' || buff[0] == '#') // Newline or comment
			throw_parse_error("Incomplete directive: `", name, '`');
		if (buff[0] != '=' && buff[0] != ':')
			throw_parse_error("Wrong assignment operator: `", buff[0], '`');

		buff.removePrefix(1); // Assignment operator
		buff.removeLeading(isWs);

		/* Value */
		Variable *varp;
		if (load_all)
			varp = &vars[name.to_string()];
		else {
			auto it = vars.find(name.to_string());
			varp = (it == vars.end() ? &tmp : &it->second);
		}
		Variable& var = *varp;
		if (var.isSet())
			var.unset();
		var.flag = Variable::SET;
		// Normal
		if (buff[0] != '[') {
			if (buff[0] == '\n' || buff[0] == '#') // No value
				var.s.clear();
			else
				var.s = extract_value();
			DEBUG_CF(stdlog("  value: ", escapeToDoubleQuotedString(var.s));)
		// Array
		} else {
			DEBUG_CF(stdlog("Array:");)
			var.flag |= Variable::ARRAY;
			buff.removePrefix(1); // Skip [
			while (buff.size()) {
				buff.removeLeading(isspace);

				if (buff[0] == ']') { // End of the array
					buff.removePrefix(1);
					break;
				}
				if (buff[0] == '#') { // Comment
					skip_comment();
					continue;
				}
				// Ignore extra delimiters
				if (buff[0] == ',') {
					buff.removePrefix(1);
					continue;
				}

				// Value
				var.a.emplace_back(extract_value());
				DEBUG_CF(stdlog("->\t",
					escapeToDoubleQuotedString(var.a.back()));)

				buff.removeLeading(isWs);
				// Delimiter
				if (buff[0] == ',' || buff[0] == '\n') {
					buff.removePrefix(1);
					continue;
				}
				// Comment
				if (buff[0] == '#') {
					skip_comment();
					continue;
				}
				// End of the array
				if (buff[0] == ']') {
					buff.removePrefix(1);
					break;
				}

				// Error - unknown sequence after the value
				throw_parse_error("Unknown sequence after the value: `", buff[0],
					'`');
			}

			if (buff.empty())
				throw_parse_error("Missing terminating ] character at the end "
					"of an array");
		}

		/* After the value */
		buff.removeLeading(isWs);
		if (buff[0] == '#') {
			skip_comment();
			continue;
		}

		// Error - unknown sequence after the value
		if (buff[0] != '\n')
			throw_parse_error("Unknown sequence after the value: `", buff[0],
				'`');

		buff.removePrefix(1); // Newline
	}

	DEBUG_CF(stdlog("End of the config file.");)
}

bool ConfigFile::isStringLiteral(const StringView& str) {
	if (str.empty())
		return false;

	// Special check on the first and last character
	if (isIn(str[0], {'[', '\'', '"', '#'}) || isspace(str[0]) ||
		isspace(str.back()))
	{
		return false;
	}

	for (char c : str)
		if (isIn(c, {'\n', ']', ',', '#'}))
			return false;

	return true;
}

string ConfigFile::escapeToSingleQuotedString(const StringView& str) {
	string res {'\''};
	res.reserve(str.size());
	for (size_t i = 0; i < str.size(); ++i) {
		res += str[i];
		if (str[i] == '\'')
			res += '\'';
	}
	return (res += '\'');
}

template<class Func>
static string _escapeToDoubleQuotedString(const StringView& str, Func&& func) {
	string res {'"'};
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
			if (func(str[i])) {
				res += "\\x";
				res += dectohex(static_cast<unsigned char>(str[i]) >> 4);
				res += dectohex(static_cast<unsigned char>(str[i]) & 15);
			} else
				res += str[i];
		}

	return (res += '"');
}

string ConfigFile::escapeToDoubleQuotedString(const StringView& str) {
	return _escapeToDoubleQuotedString(str, ::iscntrl);
}

string ConfigFile::escapeToDoubleQuotedString(const StringView& str, int) {
	return _escapeToDoubleQuotedString(str, [](int x) { return !isprint(x); });
}

string ConfigFile::escapeString(const StringView& str) {
	for (size_t i = 0; i < str.size(); ++i)
		if (str[i] == '\'' || iscntrl(str[i]))
			return escapeToDoubleQuotedString(str);

	if (isStringLiteral(str))
		return str.to_string();

	return escapeToSingleQuotedString(str);
}

string ConfigFile::escapeString(const StringView& str, int) {
	for (size_t i = 0; i < str.size(); ++i)
		if (str[i] == '\'' || !isprint(str[i]))
			return escapeToDoubleQuotedString(str, 0);

	if (isStringLiteral(str))
		return str.to_string();

	return escapeToSingleQuotedString(str);
}

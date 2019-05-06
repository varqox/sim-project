#include "../include/config_file.h"
#include "../include/debug.h"
#include "../include/filesystem.h"
#include "../include/parsers.h"
#include "../include/utilities.h"

using std::string;
using std::vector;

#if 0
# warning "Before committing disable this debug"
# define DEBUG_CF(...) __VA_ARGS__
#else
# define DEBUG_CF(...)
#endif

const ConfigFile::Variable ConfigFile::null_var;

void ConfigFile::load_config_from_file(FilePath pathname, bool load_all) {
	string contents = getFileContents(pathname);
	load_config_from_string(contents, load_all);
}

void ConfigFile::load_config_from_string(string config, bool load_all) {
	// Set all variables as unused
	for (auto it : vars)
		it.second.unset();

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
	auto curr_pos = [&]() -> size_t { return buff.data() - buff_beg; };

	auto throw_parse_error = [&](auto&&... args) {
		auto pos = curr_pos() - buff.empty();
		auto x = pos; // Position of the last newline before pos
		while (x && config[--x] != '\n') {}

		int line = 1 + std::count(buff_beg, buff_beg + x + 1, '\n');
		size_t col = pos - x + (line == 1); // Indexed from 1

		ParseError pe(line, col, std::forward<decltype(args)>(args)...);

		// Construct diagnostics
		auto& diags = pe.diagnostics_;
		auto append_char = [&](unsigned char c) {
			if (isprint(c))
				diags += c;
			else {
				diags += "\\x";
				diags += dectohex(c >> 4);
				diags += dectohex(c & 15);
			}
		};

		// Left part
		constexpr uint CONTEXT = 32;
		if (col <= CONTEXT + 1) {
			for (size_t k = x; k < pos; ++k)
				append_char(config[k]);
		} else {
			diags += "...";
			for (size_t k = pos - CONTEXT; k < pos; ++k)
				append_char(config[k]);
		}

		// Faulty position
		size_t padding = diags.size();
		uint stress_len = 1;
		if (config[pos] != '\n') {
			append_char(config[pos]);
			stress_len = diags.size() - padding;

			// Right part
			size_t k = pos + 1;
			for (; config[k] != '\n' and k <= pos + CONTEXT; ++k)
				append_char(config[k]);

			if (config[k] != '\n')
				diags += "...";
		}

		// Diagnostics's second line - stress
		diags += "\n";
		diags.append(padding, ' ');
		diags += '^';
		diags.append(stress_len - 1, '~');

		DEBUG_CF(
			stdlog("Throwing exception: ", pe.what(), "\n", pe.diagnostcs());
		)
		throw pe;
	};

	auto extract_value = [&](bool is_in_array) {
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
		if (buff[0] == '[' ||
			(is_in_array && (buff[0] == ',' || buff[0] == ']')))
		{
			throw_parse_error("Invalid beginning of the string literal: `",
				buff[0], '`');
		}

		// Beginning is ok
		// Interior
		if (is_in_array) // Value in an array
			while (buff[i + 1] != '\n' && buff[i + 1] != '#' &&
				buff[i + 1] != ']' && buff[i + 1] != ',')
			{
				++i;
			}

		else // Value of an ordinary variable
			while (buff[i + 1] != '\n' && buff[i + 1] != '#')
				++i;

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
			throw_parse_error("Invalid or missing variable's name");

		DEBUG_CF(stdlog("Variable: ", name);)

		/* Assignment operator */
		buff.removeLeading(isWs);
		if (buff[0] == '\n' || buff[0] == '#') // Newline or comment
			throw_parse_error("Incomplete directive: `", name, '`');
		if (buff[0] != '=' && buff[0] != ':')
			throw_parse_error("Invalid assignment operator: `", buff[0], '`');

		buff.removePrefix(1); // Assignment operator
		buff.removeLeading(isWs);

		/* Value */

		Variable *varp; // It is safe to take a pointer to a value in vars, as
		                // vars is not modified after that
		if (load_all)
			varp = &vars[name.to_string()];
		else {
			auto it = vars.find(name.to_string());
			varp = (it != vars.end() ? &it->second : &tmp);
		}
		Variable& var = *varp;
		if (var.is_set())
			var.unset();

		var.flag = Variable::SET;
		var.value_span.beg = curr_pos();

		// Normal
		if (buff[0] != '[') {
			if (buff[0] == '\n' || buff[0] == '#') // No value
				var.s.clear();
			else
				var.s = extract_value(false);
			DEBUG_CF(stdlog("  value: ", escape_to_double_quoted_string(var.s));)
		// Array
		} else {
			DEBUG_CF(stdlog("Array:");)
			var.flag |= Variable::ARRAY;
			buff.removePrefix(1); // Skip [

			for (;;) {
				buff.removeLeading(isspace);
				if (buff.empty()) {
					throw_parse_error("Missing terminating ] character at the"
						" end of an array");
				}

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
				var.a.emplace_back(extract_value(true));
				DEBUG_CF(stdlog("->\t",
					escape_to_double_quoted_string(var.a.back()));)

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
		}

		// It is not obvious that the below line is correct, but after the
		// analysis of the above parsing it is correct (see the loop breaks)
		var.value_span.end = curr_pos();

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

bool ConfigFile::is_string_literal(StringView str) noexcept {
	if (str.empty())
		return false;

	// Special check on the first and last character
	if (isOneOf(str[0], '[', '\'', '"', '#') || isspace(str[0]) ||
		isspace(str.back()))
	{
		return false;
	}

	for (char c : str)
		if (isOneOf(c, '\n', ']', ',', '#'))
			return false;

	return true;
}

string ConfigFile::escape_to_single_quoted_string(StringView str) {
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
static string _escape_to_double_quoted_string(StringView str, Func&& func) {
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

string ConfigFile::escape_to_double_quoted_string(StringView str) {
	return _escape_to_double_quoted_string(str, ::iscntrl);
}

string ConfigFile::escape_to_double_quoted_string(StringView str, int) {
	return _escape_to_double_quoted_string(str, [](int x) { return !isprint(x); });
}

string ConfigFile::escape_string(StringView str) {
	for (size_t i = 0; i < str.size(); ++i)
		if (str[i] == '\'' || iscntrl(str[i]))
			return escape_to_double_quoted_string(str);

	if (is_string_literal(str))
		return str.to_string();

	return escape_to_single_quoted_string(str);
}

string ConfigFile::escape_string(StringView str, int) {
	for (size_t i = 0; i < str.size(); ++i)
		if (str[i] == '\'' || !isprint(str[i]))
			return escape_to_double_quoted_string(str, 0);

	if (is_string_literal(str))
		return str.to_string();

	return escape_to_single_quoted_string(str);
}

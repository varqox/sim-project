#include "simlib/config_file.hh"
#include "simlib/ctype.hh"
#include "simlib/debug.hh"
#include "simlib/file_contents.hh"
#include "simlib/simple_parser.hh"
#include "simlib/utilities.hh"

#include <utility>

using std::string;

#if 0
#warning "Before committing disable this debug"
#define DEBUG_CF(...) __VA_ARGS__
#else
#define DEBUG_CF(...)
#endif

void ConfigFile::load_config_from_file(FilePath pathname, bool load_all) {
	string contents = get_file_contents(pathname);
	load_config_from_string(contents, load_all);
}

void ConfigFile::load_config_from_string(string config, bool load_all) {
	// Set all variables as unused
	for (auto it : vars) {
		it.second.unset();
	}

	// Checks whether c is a white-space but not a newline
	auto is_ws = [](auto c) { return (c != '\n' and is_space(c)); };
	// Checks whether character is one of these [a-zA-Z0-9\-_.]
	auto is_name = [](auto c) {
		return (is_alnum(c) or c == '-' or c == '_' or c == '.');
	};

	DEBUG_CF(stdlog("Config file:\n", config);)

	config += '\n'; // Now each line ends with a newline character
	SimpleParser buff{config};

	Variable tmp; // Used for ignored variables
	auto buff_beg = buff.data();
	auto curr_pos = [&]() -> size_t { return buff.data() - buff_beg; };

	auto throw_parse_error = [&](auto&&... args) {
		auto pos = curr_pos() - buff.empty();
		auto x = pos; // Position of the last newline before pos
		while (x and config[--x] != '\n') {
		}

		auto line = 1 + std::count(buff_beg, buff_beg + x + 1, '\n');
		size_t col = pos - x + (line == 1); // Indexed from 1

		ParseError pe(line, col, std::forward<decltype(args)>(args)...);

		// Construct diagnostics
		auto& diags = pe.diagnostics_;
		auto append_char = [&](unsigned char c) {
			if (is_print(c)) {
				diags += c;
			} else {
				diags += "\\x";
				diags += dec2hex(c >> 4);
				diags += dec2hex(c & 15);
			}
		};

		// Left part
		constexpr uint CONTEXT = 32;
		if (col <= CONTEXT + 1) {
			for (size_t k = x; k < pos; ++k) {
				append_char(config[k]);
			}
		} else {
			diags += "...";
			for (size_t k = pos - CONTEXT; k < pos; ++k) {
				append_char(config[k]);
			}
		}

		// Faulty position
		size_t padding = diags.size();
		uint stress_len = 1;
		if (config[pos] != '\n') {
			append_char(config[pos]);
			stress_len = diags.size() - padding;

			// Right part
			size_t k = pos + 1;
			for (; config[k] != '\n' and k <= pos + CONTEXT; ++k) {
				append_char(config[k]);
			}

			if (config[k] != '\n') {
				diags += "...";
			}
		}

		// Diagnostics's second line - stress
		diags += "\n";
		diags.append(padding, ' ');
		diags += '^';
		diags.append(stress_len - 1, '~');

		DEBUG_CF(
			stdlog("Throwing exception: ", pe.what(), "\n", pe.diagnostics());)
		throw std::move(pe);
	};

	auto extract_value = [&](bool is_in_array) {
		string res;
		size_t i = 0;
		// Single-quoted string
		if (buff[0] == '\'') {
			while (buff[++i] != '\n') {
				if (buff[i] == '\'') {
					if (buff[i + 1] != '\'') { // Safe (newline is at the end
											   // of every line)
						buff.remove_prefix(i + 1);
						return res;
					}

					++i;
				}
				res += buff[i];
			}
			buff.remove_prefix(i);
			throw_parse_error("Missing terminating ' character");
		}

		// Double-quoted string
		if (buff[0] == '"') {
			while (buff[++i] != '\n') {
				if (buff[i] == '"') {
					buff.remove_prefix(i + 1);
					return res;
				}

				if (buff[i] != '\\') {
					res += buff[i];
					continue;
				}

				// Escape sequence
				switch (buff[++i]) {
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
					if (!is_xdigit(buff[++i])) {
						buff.remove_prefix(i);
						throw_parse_error(
							"Invalid hexadecimal digit: `", buff[0], '`');
					}
					if (!is_xdigit(buff[++i])) {
						buff.remove_prefix(i);
						throw_parse_error(
							"Invalid hexadecimal digit: `", buff[0], '`');
					}
					res += static_cast<char>(
						(hex2dec(buff[i - 1]) << 4) + hex2dec(buff[i]));
					continue;
				default:
					buff.remove_prefix(i);
					throw_parse_error(
						"Unknown escape sequence: `\\", buff[0], '`');
				}
			}
			buff.remove_prefix(i);
			throw_parse_error("Missing terminating \" character");
		}

		// String literal

		// Check if the string literal begins
		if (buff[0] == '[' or
			(is_in_array and (buff[0] == ',' or buff[0] == ']'))) {
			throw_parse_error(
				"Invalid beginning of the string literal: `", buff[0], '`');
		}

		// Beginning is ok
		// Interior
		if (is_in_array) { // Value in an array
			while (buff[i + 1] != '\n' and buff[i + 1] != '#' and
				   buff[i + 1] != ']' and buff[i + 1] != ',')
			{
				++i;
			}

		} else { // Value of an ordinary variable
			while (buff[i + 1] != '\n' and buff[i + 1] != '#') {
				++i;
			}
		}

		// Remove white-spaces from ending
		while (is_space(buff[i])) {
			--i;
		}
		res = buff.substring(0, ++i).to_string();

		buff.remove_prefix(i);
		return res;
	};

	while (!buff.empty()) {
		auto skip_comment = [&] {
			buff.remove_leading([](char c) { return (c != '\n'); });
		};

		buff.remove_leading(is_ws);
		// Newline
		if (buff[0] == '\n') {
			buff.remove_prefix(1);
			continue;
		}
		// Comment
		if (buff[0] == '#') {
			skip_comment();
			buff.remove_prefix(1);
			continue;
		}

		/* Variable name */
		StringView name = buff.extract_leading(is_name);
		if (name.empty()) {
			throw_parse_error("Invalid or missing variable's name");
		}

		DEBUG_CF(stdlog("Variable: ", name);)

		/* Assignment operator */
		buff.remove_leading(is_ws);
		if (buff[0] == '\n' or buff[0] == '#') { // Newline or comment
			throw_parse_error("Incomplete directive: `", name, '`');
		}
		if (buff[0] != '=' and buff[0] != ':') {
			throw_parse_error("Invalid assignment operator: `", buff[0], '`');
		}

		buff.remove_prefix(1); // Assignment operator
		buff.remove_leading(is_ws);

		/* Value */

		Variable* varp = nullptr; // It is safe to take a pointer to a value in
								  // vars, as vars is not modified after that
		if (load_all) {
			varp = &vars[name.to_string()];
		} else {
			auto it = vars.find(name.to_string());
			varp = (it != vars.end() ? &it->second : &tmp);
		}
		Variable& var = *varp;
		if (var.is_set()) {
			var.unset();
		}

		var.flag_ = Variable::SET;
		var.value_span_.beg = curr_pos();

		if (buff[0] != '[') { // Normal
			if (buff[0] == '\n' or buff[0] == '#') { // No value
				var.str_.clear();
			} else {
				var.str_ = extract_value(false);
			}
			DEBUG_CF(
				stdlog("  value: ", escape_to_double_quoted_string(var.str_));)
		} else { // Array
			DEBUG_CF(stdlog("Array:");)
			var.flag_ |= Variable::ARRAY;
			buff.remove_prefix(1); // Skip [

			for (;;) {
				buff.remove_leading(is_space<char>);
				if (buff.empty()) {
					throw_parse_error("Missing terminating ] character at the"
									  " end of an array");
				}

				if (buff[0] == ']') { // End of the array
					buff.remove_prefix(1);
					break;
				}
				if (buff[0] == '#') { // Comment
					skip_comment();
					continue;
				}
				// Ignore extra delimiters
				if (buff[0] == ',') {
					buff.remove_prefix(1);
					continue;
				}

				// Value
				var.arr_.emplace_back(extract_value(true));
				DEBUG_CF(stdlog(
							 "->\t",
							 escape_to_double_quoted_string(var.arr_.back()));)

				buff.remove_leading(is_ws);
				// Delimiter
				if (buff[0] == ',' or buff[0] == '\n') {
					buff.remove_prefix(1);
					continue;
				}
				// Comment
				if (buff[0] == '#') {
					skip_comment();
					continue;
				}
				// End of the array
				if (buff[0] == ']') {
					buff.remove_prefix(1);
					break;
				}

				// Error - unknown sequence after the value
				throw_parse_error(
					"Unknown sequence after the value: `", buff[0], '`');
			}
		}

		// It is not obvious that the below line is correct, but after the
		// analysis of the above parsing it is correct (see the loop breaks)
		var.value_span_.end = curr_pos();

		/* After the value */
		buff.remove_leading(is_ws);
		if (buff[0] == '#') {
			skip_comment();
			continue;
		}

		// Error - unknown sequence after the value
		if (buff[0] != '\n') {
			throw_parse_error(
				"Unknown sequence after the value: `", buff[0], '`');
		}

		buff.remove_prefix(1); // Newline
	}

	DEBUG_CF(stdlog("End of the config file.");)
}

bool ConfigFile::is_string_literal(StringView str) noexcept {
	if (str.empty()) {
		return false;
	}

	// Special check on the first and last character
	if (is_one_of(str[0], '[', '\'', '"', '#') or is_space(str[0]) or
		is_space(str.back()))
	{
		return false;
	}

	for (auto c : str) {
		if (is_one_of(c, '\n', ']', ',', '#')) {
			return false;
		}
	}

	return true;
}

string ConfigFile::escape_to_single_quoted_string(StringView str) {
	string res{'\''};
	res.reserve(str.size());
	for (size_t i = 0; i < str.size(); ++i) {
		res += str[i];
		if (str[i] == '\'') {
			res += '\'';
		}
	}
	return (res += '\'');
}

template <class Func>
static string escape_to_double_quoted_string_impl(StringView str, Func&& func) {
	string res{'"'};
	res.reserve(str.size());
	for (size_t i = 0; i < str.size(); ++i) {
		switch (str[i]) {
		case '\\': res += "\\\\"; break;
		case '\0': res += "\\0"; break;
		case '"': res += "\\\""; break;
		case '?': res += "\\?"; break;
		case '\a': res += "\\a"; break;
		case '\b': res += "\\b"; break;
		case '\f': res += "\\f"; break;
		case '\n': res += "\\n"; break;
		case '\r': res += "\\r"; break;
		case '\t': res += "\\t"; break;
		case '\v': res += "\\v"; break;
		default:
			if (func(str[i])) {
				res += "\\x";
				res += dec2hex(static_cast<unsigned char>(str[i]) >> 4);
				res += dec2hex(static_cast<unsigned char>(str[i]) & 15);
			} else {
				res += str[i];
			}
		}
	}

	return (res += '"');
}

string ConfigFile::escape_to_double_quoted_string(StringView str) {
	return escape_to_double_quoted_string_impl(str, is_cntrl<char>);
}

string ConfigFile::full_escape_to_double_quoted_string(StringView str) {
	return escape_to_double_quoted_string_impl(
		str, [](int x) { return !is_print(x); });
}

string ConfigFile::escape_string(StringView str) {
	for (size_t i = 0; i < str.size(); ++i) {
		if (str[i] == '\'' or is_cntrl(str[i])) {
			return escape_to_double_quoted_string(str);
		}
	}

	if (is_string_literal(str)) {
		return str.to_string();
	}

	return escape_to_single_quoted_string(str);
}

string ConfigFile::full_escape_string(StringView str) {
	for (size_t i = 0; i < str.size(); ++i) {
		if (str[i] == '\'' or !is_print(str[i])) {
			return full_escape_to_double_quoted_string(str);
		}
	}

	if (is_string_literal(str)) {
		return str.to_string();
	}

	return escape_to_single_quoted_string(str);
}

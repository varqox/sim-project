#include "../include/config_file.h"
#include "../include/filesystem.h"
#include "../include/logger.h"

using std::map;
using std::string;
using std::vector;

size_t ConfigFile::parseSingleQuotedString(const StringView& in, string& out,
		size_t line) {
	for (size_t i = 1, end = in.size();; ++i) {
		if (i == end)
			throw ParseError(line, "missing terminating ' character");
		// Escape double single quote or end string
		if (in[i] == '\'') {
			++i;
			if (i == end || in[i] != '\'') // String end
				return i;
		}
		out += in[i];
	}
}

size_t ConfigFile::parseDoubleQuotedString(const StringView& in, string& out,
		size_t line) {
	for (size_t i = 1, end = in.size();; ++i) {
		if (i == end)
			throw ParseError(line, "missing terminating \" character");

		if (in[i] == '"') // String end
			return i + 1;

		if (in[i] == '\\') {
			if (++i == end)
				throw ParseError(line, "multi-line strings are not supported");

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
					throw ParseError(line, "incomplete escape sequence \\x");
				++i;
				out += static_cast<char>(hextodec(in[i]) * 16 +
					hextodec(in[i + 1]));
				++i;
				continue;
			default:
				throw ParseError(line, string("unknown escape sequence \\") +
					in[i]);
			}
			// Should never reach here
		}
		out += in[i];
	}
}

size_t ConfigFile::extractValue(const StringView& in, string& out,
		size_t line) {
	// Single quoted string
	if (in.front() == '\'')
		return parseSingleQuotedString(in, out, line);

	// Double quoted string
	if (in.front() == '"')
		return parseDoubleQuotedString(in, out, line);

	// String literal (not array)
	size_t i = 0, end = in.size();
	while (i < end && isStringLiteral(in[i]))
		out += in[i++];
	return i;
}

void ConfigFile::loadConfigFromFile(const string& pathname, bool load_all) {
	int fd;
	while ((fd = open(pathname.c_str(), O_RDONLY | O_LARGEFILE)) == -1 &&
		errno == EINTR) {}

	if (fd == -1)
		throw std::runtime_error(concat("Failed to open '", pathname, "'",
			error(errno)));

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
	auto ignoreWhitespace = [&beg, &end, &config]() {
		while (beg < end && isspace(config[beg]))
			++beg;
	};

	while ((beg = ++end) < config.size()) {
		end = std::min(config.find('\n', beg), config.size());
		++line;
		tmp.flag = 0;
		ignoreWhitespace();
		// Assert beg < end
		if (beg == end || config[beg] == '#') // Blank line or comment
			continue;

		// Extract name
		x = beg;
		while (beg < end && isName(config[beg]))
			++beg;
		string name(config.data() + x, config.data() + beg);
		if (name.empty())
			throw ParseError(line, "missing variable name");

		// Get corresponding variable
		map<string, Variable>::iterator it = (load_all ?
			vars.insert(make_pair(name, Variable())).first : vars.find(name));
		Variable& var = (it == vars.end() ? tmp : it->second);
		if (var.flag & Variable::SET)
			throw ParseError(line, concat("variable '", name,
				"' defined more than once"));

		var.flag |= Variable::SET;
		var.s.clear();
		var.a.clear();

		// Assignment operator
		ignoreWhitespace();
		// Assert beg < end
		if (beg == end)
			throw ParseError(line, "missing assignment operator");

		if (config[beg] != ':' && config[beg] != '=')
			throw ParseError(line, concat("wrong assignment operator '",
				config[beg], '\''));

		++beg; // Ignore assignment operator
		ignoreWhitespace();
		// Assert beg < end
		if (beg == end || config[beg] == '#') // No value or comment
			continue;

		// Extract value
		if (config[beg] != '[') // Value
			beg += extractValue(config.substr(beg, end - beg), var.s, line);
		else { // Array
			++beg; // Ignore '['
			var.flag |= Variable::ARRAY;
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

					var.a.push_back(string());
					beg += extractValue(config.substr(beg, end - beg),
						var.a.back(), line);
					expect_value = false;

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
			throw ParseError(line, string("Unknown character at the end of "
				"line: ") + config[beg]);
	}
}

int ConfigFile::getInt(const StringView &name) const {
	map<string, Variable>::const_iterator it = vars.find(name.to_string());
	if (it == vars.end())
		return 0;

	int x;
	return strtoi(it->second.s, &x) > 0 ? x : 0;
}

bool ConfigFile::getBool(const StringView &name) const {
	map<string, Variable>::const_iterator it = vars.find(name.to_string());
	if (it == vars.end())
		return false;

	std::string val = tolower(it->second.s);
	return (val == "1" || val == "on" || val == "true");
}

double ConfigFile::getReal(const StringView &name) const {
	map<string, Variable>::const_iterator it = vars.find(name.to_string());
	return (it == vars.end() ? 0 : strtod(it->second.s.c_str(), nullptr));
}

std::string ConfigFile::getString(const StringView &name) const {
	map<string, Variable>::const_iterator it = vars.find(name.to_string());
	return (it == vars.end() ? "" : it->second.s);
}

std::vector<std::string> ConfigFile::getArray(const StringView& name) const {
	map<string, Variable>::const_iterator it = vars.find(name.to_string());
	return (it == vars.end() ? vector<string>() : it->second.a);
}

bool ConfigFile::isStringLiteral(const StringView& str) {
	for (size_t i = 0; i < str.size(); ++i)
		if (!isStringLiteral(str[i]))
			return false;

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
		bool escape_unprintable) {
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

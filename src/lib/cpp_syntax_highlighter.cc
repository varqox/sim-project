#include <sim/cpp_syntax_highlighter.h>
#include <simlib/logger.h>
#include <simlib/utilities.h>

using std::array;
using std::string;
using std::vector;

#if 0
# warning "Before committing disable this debug"
# define DEBUG_CSH(...) __VA_ARGS__
#else
# define DEBUG_CSH(...)
#endif

typedef int8_t StyleType;

namespace {

enum Style : StyleType {
	PREPROCESSOR = 0,
	COMMENT = 1,
	BUILTIN_TYPE = 2,
	KEYWORD = 3,
	STRING_LITERAL = 4,
	CHARACTER = 5,
	ESCAPED_CHARACTER = 6,
	DIGIT = 7,
	CONSTANT = 8,
	FUNCTION = 9,
	OPERATOR = 10
};

struct Word {
	const char* str;
	uint8_t size;
	Style style;
};

} // anonymous namespace

static constexpr std::array<const char*, 11> begin_style {{
	"<span style=\"color:#00a000\">",
	"<span style=\"color:#a0a0a0\">",
	"<span style=\"color:#0000ff;font-weight:bold\">",
	"<span style=\"color:#c90049;font-weight:bold;\">",
	"<span style=\"color:#ff0000\">",
	"<span style=\"color:#e0a000\">",
	"<span style=\"color:#d923e9\">",
	"<span style=\"color:#d923e9\">",
	"<span style=\"color:#a800ff\">",
	"<span style=\"color:#0086b3\">",
	"<span style=\"color:#515125\">",
}};

static constexpr const char* end_style = "</span>";

static constexpr std::array<Word, 123> words {{
	{"uint_least16_t", 14, BUILTIN_TYPE},
	{"uint_least32_t", 14, BUILTIN_TYPE},
	{"uint_least64_t", 14, BUILTIN_TYPE},
	{"uint_fast16_t", 13, BUILTIN_TYPE},
	{"uint_fast64_t", 13, BUILTIN_TYPE},
	{"uint_least8_t", 13, BUILTIN_TYPE},
	{"int_least16_t", 13, BUILTIN_TYPE},
	{"int_least32_t", 13, BUILTIN_TYPE},
	{"int_least64_t", 13, BUILTIN_TYPE},
	{"uint_fast32_t", 13, BUILTIN_TYPE},
	{"uint_fast8_t", 12, BUILTIN_TYPE},
	{"int_fast16_t", 12, BUILTIN_TYPE},
	{"int_least8_t", 12, BUILTIN_TYPE},
	{"int_fast32_t", 12, BUILTIN_TYPE},
	{"int_fast64_t", 12, BUILTIN_TYPE},
	{"int_fast8_t", 11, BUILTIN_TYPE},
	{"_Char16_t", 9, BUILTIN_TYPE},
	{"_Char32_t", 9, BUILTIN_TYPE},
	{"uintptr_t", 9, BUILTIN_TYPE},
	{"uintmax_t", 9, BUILTIN_TYPE},
	{"wint_t", 6, BUILTIN_TYPE},
	{"wctrans_t", 9, BUILTIN_TYPE},
	{"unsigned", 8, BUILTIN_TYPE},
	{"uint16_t", 8, BUILTIN_TYPE},
	{"uint32_t", 8, BUILTIN_TYPE},
	{"uint64_t", 8, BUILTIN_TYPE},
	{"intmax_t", 8, BUILTIN_TYPE},
	{"wctype_t", 8, BUILTIN_TYPE},
	{"intptr_t", 8, BUILTIN_TYPE},
	{"wchar_t", 7, BUILTIN_TYPE},
	{"uint8_t", 7, BUILTIN_TYPE},
	{"int16_t", 7, BUILTIN_TYPE},
	{"int32_t", 7, BUILTIN_TYPE},
	{"int64_t", 7, BUILTIN_TYPE},
	{"wchar_t", 7, BUILTIN_TYPE},
	{"double", 6, BUILTIN_TYPE},
	{"signed", 6, BUILTIN_TYPE},
	{"size_t", 6, BUILTIN_TYPE},
	{"time_t", 6, BUILTIN_TYPE},
	{"int8_t", 6, BUILTIN_TYPE},
	{"short", 5, BUILTIN_TYPE},
	{"float", 5, BUILTIN_TYPE},
	{"void", 4, BUILTIN_TYPE},
	{"char", 4, BUILTIN_TYPE},
	{"bool", 4, BUILTIN_TYPE},
	{"long", 4, BUILTIN_TYPE},
	{"int", 3, BUILTIN_TYPE},
	{"nullptr_t", 9, BUILTIN_TYPE},
	{"auto", 4, BUILTIN_TYPE},
	{"enum", 4, KEYWORD},
	{"class", 5, KEYWORD},
	{"union", 5, KEYWORD},
	{"struct", 6, KEYWORD},
	{"namespace", 9, KEYWORD},
	{"or", 2, KEYWORD},
	{"if", 2, KEYWORD},
	{"do", 2, KEYWORD},
	{"for", 3, KEYWORD},
	{"asm", 3, KEYWORD},
	{"and", 3, KEYWORD},
	{"try", 3, KEYWORD},
	{"not", 3, KEYWORD},
	{"xor", 3, KEYWORD},
	{"new", 3, KEYWORD},
	{"goto", 4, KEYWORD},
	{"else", 4, KEYWORD},
	{"this", 4, KEYWORD},
	{"case", 4, KEYWORD},
	{"or_eq", 5, KEYWORD},
	{"final", 5, KEYWORD},
	{"using", 5, KEYWORD},
	{"while", 5, KEYWORD},
	{"bitor", 5, KEYWORD},
	{"compl", 5, KEYWORD},
	{"const", 5, KEYWORD},
	{"catch", 5, KEYWORD},
	{"break", 5, KEYWORD},
	{"sizeof", 6, KEYWORD},
	{"static", 6, KEYWORD},
	{"switch", 6, KEYWORD},
	{"return", 6, KEYWORD},
	{"and_eq", 6, KEYWORD},
	{"bitand", 6, KEYWORD},
	{"not_eq", 6, KEYWORD},
	{"xor_eq", 6, KEYWORD},
	{"typeid", 6, KEYWORD},
	{"throw", 5, KEYWORD},
	{"extern", 6, KEYWORD},
	{"export", 6, KEYWORD},
	{"friend", 6, KEYWORD},
	{"import", 6, KEYWORD},
	{"public", 6, KEYWORD},
	{"inline", 6, KEYWORD},
	{"delete", 6, KEYWORD},
	{"typedef", 7, KEYWORD},
	{"alignof", 7, KEYWORD},
	{"mutable", 7, KEYWORD},
	{"decltype", 8, KEYWORD},
	{"continue", 8, KEYWORD},
	{"explicit", 8, KEYWORD},
	{"default", 7, KEYWORD},
	{"nullptr", 7, KEYWORD},
	{"private", 7, KEYWORD},
	{"virtual", 7, KEYWORD},
	{"volatile", 8, KEYWORD},
	{"protected", 9, KEYWORD},
	{"constexpr", 9, KEYWORD},
	{"noexcept", 8, KEYWORD},
	{"operator", 8, KEYWORD},
	{"override", 8, KEYWORD},
	{"template", 8, KEYWORD},
	{"register", 8, KEYWORD},
	{"typename", 8, KEYWORD},
	{"static_cast", 11, KEYWORD},
	{"const_cast", 10, KEYWORD},
	{"align_union", 11, KEYWORD},
	{"dynamic_cast", 12, KEYWORD},
	{"reinterpret_cast", 16, KEYWORD},
	{"static_assert", 13, KEYWORD},
	{"true", 4, CONSTANT},
	{"false", 5, CONSTANT},
	{"NULL", 4, CONSTANT},
	{"nullptr", 7, CONSTANT},
}};

// Important: elements have to be sorted!
static constexpr array<const char*, 64> cpp_keywords {{
	"align_union", "alignof", "and", "and_eq", "asm", "bitand", "bitor",
	"break", "case", "catch", "compl", "const", "const_cast", "constexpr",
	"continue", "decltype", "default", "delete", "do", "dynamic_cast", "else",
	"explicit", "export", "extern", "final", "for", "friend", "goto", "if",
	"import", "inline", "mutable", "new", "not", "not_eq", "nullptr",
	"operator", "or", "or_eq", "override", "private", "protected", "public",
	"register", "reinterpret_cast", "return", "sizeof", "static",
	"static_assert", "static_cast", "switch", "template", "this", "throw",
	"try", "typedef", "typeid", "typename", "using", "virtual", "volatile",
	"while", "xor", "xor_eq"
}};


CppSyntaxHighlighter::CppSyntaxHighlighter() {
	for (uint i = 0; i < words.size(); ++i)
		aho.addPattern(words[i].str, i + 1);

	aho.buildFails();
}

string CppSyntaxHighlighter::operator()(const StringView& str) const {
	int size = str.size(); // We can use int - this function shuld not be called
	                       // on insanely long strings
	vector<StyleType> begs(size, -1); // here beginnings of styles are marked
	vector<int8_t> ends(size + 2); // ends[i] = # of endings (of styles) just
	                               // BEFORE str[i]

	DEBUG_CSH(stdlog("size: ", toString(size));)

	/* Mark comments string / character literals */

	for (int i = 0; i < size; ++i) {
		// Comments
		if (str[i] == '/') {
			// Parse through stupid "\\\n" sequences
			int k = i + 1;
			while (str[k] == '\\' && k + 1 < size && str[k + 1] == '\n')
				k += 2;
			// (One-)line comment
			if (str[k] == '/') {
				begs[i] = COMMENT;
				i = k + 1;
				while (i < size && (str[i] != '\n' || str[i - 1] == '\\'))
					++i;
				++ends[i];

			// Multi-line comment
			} else if (str[k] == '*') {
				begs[i] = COMMENT;
				i = k + 2; // +2 is necessary to not end comment on this: /*/
				while (i < size && !(str[i] == '/' && (str[i - 1] == '*' ||
					(str[i - 1] == '\n' && str[i - 2] == '\\' &&
						str[i - 3] == '*'))))
				{
					++i;
				}

				++ends[i + 1];
			}
		}

		auto omit_backslash_newlines = [&] () {
			while (i + 1 < size && str[i] == '\\' &&
				str[i + 1] == '\n')
			{
				begs[i] = OPERATOR;
				++ends[i + 1];
				i += 2;
			}
		};

		// String / character literals
		if (str[i] == '"' || str[i] == '\'') {
			char literal_delim = str[i];
			begs[i++] = (literal_delim == '"' ? STRING_LITERAL : CHARACTER);
			while (i < size && str[i] != literal_delim) {
				if (str[i] == '\\') {
					// "\\\n" sequence
					if (i + 1 < size && str[i + 1] == '\n') {
						begs[i] = OPERATOR;
						++ends[++i];
						continue;
					}

					/* Escape sequence */
					begs[i++] = ESCAPED_CHARACTER;

					omit_backslash_newlines();
					if (i < size) {
						// Octals and Hexadecimal
						if (isdigit(str[i]) || str[i] == 'x') {
							for (int j = 0; j < 2; ++j)
								++i, omit_backslash_newlines();

						// Unicode U+nnnn
						} else if(str[i] == 'u') {
							for (int j = 0; j < 4; ++j)
								++i, omit_backslash_newlines();

						// Unicode U+nnnnnnnn
						} else if(str[i] == 'U') {
							for (int j = 0; j < 8; ++j)
								++i, omit_backslash_newlines();
						}
					}
					i = std::min(i, size - 1);
					++ends[i + 1];
				}
				++i;
			}
			++ends[i + 1];
		}
	}

	DEBUG_CSH(auto dump_begs_ends = [&] {
		for (int i = 0, line = 1; i < size; ++i) {
			auto tmplog = stdlog(toString(i), " (line ", toString(line), "): '",
				str[i], "' -> ");

			if (ends[i] == 0)
				tmplog("0, ");
			else
				tmplog("\033[1;32m", toString(ends[i]), "\033[m, ");

			switch (begs[i]) {
			case -1:
				tmplog("-1"); break;
			case PREPROCESSOR:
				tmplog("PREPROCESSOR"); break;
			case COMMENT:
				tmplog("COMMENT"); break;
			case BUILTIN_TYPE:
				tmplog("BUILTIN_TYPE"); break;
			case KEYWORD:
				tmplog("KEYWORD"); break;
			case STRING_LITERAL:
				tmplog("STRING_LITERAL"); break;
			case CHARACTER:
				tmplog("CHARACTER"); break;
			case ESCAPED_CHARACTER:
				tmplog("ESCAPED_CHARACTER"); break;
			case DIGIT:
				tmplog("DIGIT"); break;
			case CONSTANT:
				tmplog("CONSTANT"); break;
			case FUNCTION:
				tmplog("FUNCTION"); break;
			case OPERATOR:
				tmplog("OPERATOR"); break;
			default: tmplog("???");
			}

			if (str[i] == '\n')
				++line;
		}
	};)

	DEBUG_CSH(dump_begs_ends();)

	auto isName = [](int c) { return (c == '_' || c == '$' || isalnum(c)); };

	/* Mark preprocessor, functions and numbers */

	for (int i = 0, styles_depth = 0; i < size; ++i) {
		auto control_style_depth = [&] {
			if (begs[i] != -1)
				++styles_depth;
			styles_depth -= ends[i];
		};

		control_style_depth();
		if (styles_depth > 0)
			continue;

		// Preprocessor
		if (str[i] == '#') {
			begs[i] = PREPROCESSOR;

			for (++i; i < size; ++i) {
				control_style_depth();
				if (styles_depth > 0)
					continue;

				if (str[i] == '\\' && i + 1 < size && str[i + 1] == '\n') {
					begs[i] = OPERATOR;
					++i;
					control_style_depth();
					++ends[i++];
					control_style_depth();
				}
				if (str[i] == '\n')
					break;
			}

			++ends[i];

		// Function
		} else if (str[i] == '(' && i) {
			int k = i - 1;
			// Omit white-spaces between function name and '('
			while (k && (isspace(str[k]) ||
				(str[k] == '\\' && str[k + 1] == '\n')))
			{
				--k;
			}

			int kk = k + 1, last_k = kk;
			string function_name;

			// Function name
			do {
				if (k > 1 && str[k] == '\n' && str[k - 1] == '\\') {
					++ends[k];
					begs[--k] = OPERATOR;
					continue;
				}

				// Namespaces
				if (str[k] == ':' && k && str[k - 1] == ':'
					&& str[last_k] != ':')
				{
					--k;

				} else if (!isName(str[k]))
					break;

				function_name += str[k];
				last_k = k;

			} while (k--);

			// Function name cannot be blank or begin with digit
			if (last_k < kk && !isdigit(str[last_k])) {
				std::reverse(function_name.begin(), function_name.end());
				// It is important that function_name is taken as string below!
				begs[last_k] = (binary_search(cpp_keywords,
					StringView(function_name)) ? KEYWORD : FUNCTION);
				++ends[kk];
			}

		// Digits
		} else if (isdigit(str[i])) do {
			if (i > 0) {
				if (isName(str[i - 1]))
					break;
				// Floating points may begin with '.', but inhibit highlighting
				// of whole number 111.2.3
				if (str[i - 1] == '.' && i > 1 && isdigit(str[i - 2]))
					break;
			}

			// Number begins with dot
			bool dot_appeared = false, dot_as_beginning = false,
				exponent_appeared = false;
			if (i && str[i - 1] == '.') {
				dot_as_beginning = dot_appeared = true;
				begs[i - 1] = DIGIT;
			} else
				begs[i] = DIGIT;

			auto get_next = [&](int k) {
				++k;
				while (k + 1 < size && str[k] == '\\' && str[k + 1] == '\n')
					k += 2;
				return k;
			};

			int k = get_next(i);
			char last_strk = str[i];
			auto is_digit = isdigit;
			char exp_sign = 'e';

			if (last_strk == '0' && tolower(str[k]) == 'x') {
				is_digit = isxdigit;
				exp_sign = 'p';
				last_strk = str[k], k = get_next(k);
				if (str[k] == '.') {
					dot_as_beginning = dot_appeared = true;
					last_strk = str[k], k = get_next(k);

				} else if (!is_digit(str[k])) {
					begs[i - dot_as_beginning] = -1;
					i = k - 1;
					break;
				}
			}

			for (; k < size; last_strk = str[k], k = get_next(k)) {
				// Exponent
				if (is_digit(str[k]))
					continue;

				if (str[k] == '.') {
					if (dot_appeared || exponent_appeared)
						goto kill_try;

					dot_appeared = true;
				} else if (tolower(str[k]) == exp_sign) {
					// Do not allow cases like: 0.1e1e or .e2
					if (exponent_appeared ||
						(last_strk == '.' && dot_as_beginning))
					{
						goto kill_try;
					}

					exponent_appeared = true;

				} else if (last_strk == exp_sign &&
					(str[k] == '-' || str[k] == '+'))
				{
					continue;

				} else
					break;
			}

			if ((last_strk == '.' && dot_as_beginning) ||
				(exp_sign == 'p' && dot_appeared && !exponent_appeared))
			{
			kill_try:
				begs[i - (dot_as_beginning && exp_sign == 'e')] = -1;
				i = k - 1;
				break;
			}

			// Suffixes
			// Float-point: (f|l)
			if (dot_appeared || exponent_appeared) {
				i = k;
				char c = tolower(UNLIKELY(k == size) ? '\0' : str[k]);
				if (c == 'f' || c == 'l')
					++i;

			// Integer (l|ll|lu|llu|u|ul|ull)
			} else {
				i = k;
				char c = (UNLIKELY(k == size) ? '\0' : str[k]);
				if (c == 'f' || c == 'F') {
					++i;

				} else if (c == 'l' || c == 'L') { // l
					++i;
					k = get_next(k);
					if (UNLIKELY(k == size)) {

					} else if (str[k] == 'u' || str[k] == 'U') // lu
						i = k + 1;
					// Because lL and Ll suffixes are wrong
					else if (str[k] == c) { // ll | LL
						i = k + 1;
						k = get_next(k);
						if (UNLIKELY(k == size)) {

						} else if (str[k] == 'u' || str[k] == 'U') // llu
							i = k + 1;
					}

				} else if (c == 'u' || c == 'U') { // u
					++i;
					k = get_next(k);
					c = (UNLIKELY(k == size) ? '\0' : str[k]);
					if (c == 'l' || c == 'L') { // ul
						i = k + 1;
						k = get_next(k);
						// Because lL and Ll suffixes are wrong
						if (LIKELY(k < size) && str[k] == c) // ull | uLL
							i = k + 1;
					}
				}

			}

			++ends[i];

		} while (false);
	}

	DEBUG_CSH(dump_begs_ends();)

	auto isOperator = [](unsigned char c) {
		// In xxx are marked characters from "!%&()*+,-./:;<=>?[\\]^{|}~";
		constexpr array<bool, 128> xxx {{
		//      0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
		/* 0 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		/* 1 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		/* 2 */ 0, 1, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
		/* 3 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,
		/* 4 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		/* 5 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0,
		/* 6 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		/* 7 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0,
		}};
		return (c < 128 && xxx[c]);
	};

	/* Highlight non-styled code */

	// Highlights fragments of code not containing: comments, preprocessor,
	// number literals, string literals, character literals
	auto highlight = [&](int beg, int end) {
		DEBUG_CSH(stdlog("highlight(", toString(beg), ", ", toString(end),
			"):");)
		if (beg >= end)
			return;

		auto aho_res = aho.searchIn(str.substring(beg, end));
		// Handle last one to eliminate right boundary checks
		int i = end - beg - 1;
		int k = aho.pattId(aho_res[i]);
		if (k && (end == size || !isName(str[end]))) {
			int j = end - words[--k].size;
			if (j == 0) {
				begs[0] = words[k].style;
				++ends[end];
				i = j - beg;
			} else if (!isName(str[j - 1])) {
				begs[j - 1] = words[k].style;
				++ends[end];
				i = j - beg;
			}

		} else if (isOperator(str[end - 1])) {
				begs[end - 1] = OPERATOR;
				++ends[end];
		}

		while (i--)
			if ((k = aho.pattId(aho_res[i])) && !isName(str[beg + i + 1])) {
				int j = i - words[--k].size;
				if (UNLIKELY(j + beg < 0)) {
					begs[0] = words[k].style;
					++ends[beg + i + 1];
					i = 0;
				} else if (!isName(str[beg + j])) {
					begs[beg + j + 1] = words[k].style;
					++ends[beg + i + 1];
					i = j + 1;
				}
				DEBUG_CSH(stdlog("aho: ", toString(i + beg), ": ",
					words[k - 1].str);)

			} else if (isOperator(str[beg + i])) {
				begs[beg + i] = OPERATOR;
				++ends[beg + i + 1];
			}
	};

	// Find non-styled segments
	int last_non_styled = 0;
	for (int i = 0, styles_depth = 0; i < size; ++i) {
		int k = styles_depth;
		if (begs[i] != -1)
			++styles_depth;
		styles_depth -= ends[i];

		if (k == 0 && styles_depth > 0) {
			highlight(last_non_styled, i);
			// Needed for proper non-styled interval after this loop
			last_non_styled = size;

		} else if (k > 0 && styles_depth == 0)
			last_non_styled = i;
	}
	highlight(last_non_styled, size); // Take care of last non-styled piece

	DEBUG_CSH(dump_begs_ends();)

	/* Parse styles and produce result */
	string res = "<table class=\"code-view\">\n"
		"<tbody>\n"
		"<tr><td id=\"L1\" line=\"1\"></td><td>";
	// Stack of styles (needed to properly break on '\n')
	vector<StyleType> style_stack;
	int first_unescaped = 0, line = 1;
	for (int i = 0; i < size; ++i) {
		// End styles
		if (ends[i]) {
			htmlSpecialChars(res, str.substring(first_unescaped, i));
			first_unescaped = i;

			while (ends[i] > 1) {
				style_stack.pop_back();
				res += end_style;
				--ends[i];
			}

			if (ends[i]) {
				// Style elision (to compress and simplify output)
				if (style_stack.back() == begs[i])
					begs[i] = -1;
				else {
					style_stack.pop_back();
					res += end_style;
				}
			}
		}

		// Begins style
		if (begs[i] != -1) {
			htmlSpecialChars(res, str.substring(first_unescaped, i));
			first_unescaped = i;

			style_stack.emplace_back(begs[i]);
			res += begin_style[begs[i]];
		}

		// Line ending
		if (str[i] == '\n') {
			htmlSpecialChars(res, str.substring(first_unescaped, i));
			first_unescaped = i + 1;
			// End styles
			for (int k = style_stack.size(); k > 0; --k)
				res += end_style;
			// Break the line
			string line_str = toString(++line);
			back_insert(res, "</td></tr>\n"
				"<tr><td id=\"L", line_str, "\" line=\"", line_str,
				"\"></td><td>");
			// Restore styles
			for (StyleType style : style_stack)
				res += begin_style[style];
		}
	}
	// Ending
	htmlSpecialChars(res, str.substring(first_unescaped, size));
	while (ends[size]--)
		res += end_style;
	while (ends[size + 1]--)
		res += end_style;
	res += "</td></tr>\n"
			"</tbody>\n"
		"</table>\n";

	return res;
}

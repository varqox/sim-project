#include <limits>
#include <sim/cpp_syntax_highlighter.hh>
#include <simlib/debug.hh>
#include <simlib/logger.hh>
#include <simlib/meta.hh>
#include <simlib/string_transform.hh>
#include <simlib/string_view.hh>
#include <simlib/utilities.hh>

using std::array;
using std::string;
using std::vector;

#if 0
#warning "Before committing disable this debug"
#define DEBUG_CSH(...) __VA_ARGS__
#include <cassert>
#undef throw_assert
#define throw_assert assert
#else
#define DEBUG_CSH(...)
#endif

namespace {

using StyleType = int8_t;

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
    const char* const str;
    uint8_t size;
    Style style;

    template <uint8_t N>
    constexpr Word(const char (&s)[N], Style stl) : str(s)
                                                  , size(N - 1)
                                                  , style(stl) {}
};

constexpr array<CStringView, 11> begin_style{{
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

constexpr CStringView end_style = "</span>";

constexpr array<Word, 124> words{{
    {"", COMMENT}, // Guard - ignored
    {"uint_least16_t", BUILTIN_TYPE},
    {"uint_least32_t", BUILTIN_TYPE},
    {"uint_least64_t", BUILTIN_TYPE},
    {"uint_fast16_t", BUILTIN_TYPE},
    {"uint_fast64_t", BUILTIN_TYPE},
    {"uint_least8_t", BUILTIN_TYPE},
    {"int_least16_t", BUILTIN_TYPE},
    {"int_least32_t", BUILTIN_TYPE},
    {"int_least64_t", BUILTIN_TYPE},
    {"uint_fast32_t", BUILTIN_TYPE},
    {"uint_fast8_t", BUILTIN_TYPE},
    {"int_fast16_t", BUILTIN_TYPE},
    {"int_least8_t", BUILTIN_TYPE},
    {"int_fast32_t", BUILTIN_TYPE},
    {"int_fast64_t", BUILTIN_TYPE},
    {"int_fast8_t", BUILTIN_TYPE},
    {"_Char16_t", BUILTIN_TYPE},
    {"_Char32_t", BUILTIN_TYPE},
    {"uintptr_t", BUILTIN_TYPE},
    {"uintmax_t", BUILTIN_TYPE},
    {"wint_t", BUILTIN_TYPE},
    {"wctrans_t", BUILTIN_TYPE},
    {"unsigned", BUILTIN_TYPE},
    {"uint16_t", BUILTIN_TYPE},
    {"uint32_t", BUILTIN_TYPE},
    {"uint64_t", BUILTIN_TYPE},
    {"intmax_t", BUILTIN_TYPE},
    {"wctype_t", BUILTIN_TYPE},
    {"intptr_t", BUILTIN_TYPE},
    {"wchar_t", BUILTIN_TYPE},
    {"uint8_t", BUILTIN_TYPE},
    {"int16_t", BUILTIN_TYPE},
    {"int32_t", BUILTIN_TYPE},
    {"int64_t", BUILTIN_TYPE},
    {"wchar_t", BUILTIN_TYPE},
    {"double", BUILTIN_TYPE},
    {"signed", BUILTIN_TYPE},
    {"size_t", BUILTIN_TYPE},
    {"time_t", BUILTIN_TYPE},
    {"int8_t", BUILTIN_TYPE},
    {"short", BUILTIN_TYPE},
    {"float", BUILTIN_TYPE},
    {"void", BUILTIN_TYPE},
    {"char", BUILTIN_TYPE},
    {"bool", BUILTIN_TYPE},
    {"long", BUILTIN_TYPE},
    {"int", BUILTIN_TYPE},
    {"nullptr_t", BUILTIN_TYPE},
    {"auto", BUILTIN_TYPE},
    {"align_union", KEYWORD},
    {"alignof", KEYWORD},
    {"and", KEYWORD},
    {"and_eq", KEYWORD},
    {"asm", KEYWORD},
    {"bitand", KEYWORD},
    {"bitor", KEYWORD},
    {"break", KEYWORD},
    {"case", KEYWORD},
    {"catch", KEYWORD},
    {"class", KEYWORD},
    {"compl", KEYWORD},
    {"const", KEYWORD},
    {"const_cast", KEYWORD},
    {"constexpr", KEYWORD},
    {"continue", KEYWORD},
    {"decltype", KEYWORD},
    {"default", KEYWORD},
    {"delete", KEYWORD},
    {"do", KEYWORD},
    {"dynamic_cast", KEYWORD},
    {"else", KEYWORD},
    {"enum", KEYWORD},
    {"explicit", KEYWORD},
    {"export", KEYWORD},
    {"extern", KEYWORD},
    {"final", KEYWORD},
    {"for", KEYWORD},
    {"friend", KEYWORD},
    {"goto", KEYWORD},
    {"if", KEYWORD},
    {"import", KEYWORD},
    {"inline", KEYWORD},
    {"mutable", KEYWORD},
    {"namespace", KEYWORD},
    {"new", KEYWORD},
    {"noexcept", KEYWORD},
    {"not", KEYWORD},
    {"not_eq", KEYWORD},
    {"nullptr", KEYWORD},
    {"operator", KEYWORD},
    {"or", KEYWORD},
    {"or_eq", KEYWORD},
    {"override", KEYWORD},
    {"private", KEYWORD},
    {"protected", KEYWORD},
    {"public", KEYWORD},
    {"register", KEYWORD},
    {"reinterpret_cast", KEYWORD},
    {"return", KEYWORD},
    {"sizeof", KEYWORD},
    {"static", KEYWORD},
    {"static_assert", KEYWORD},
    {"static_cast", KEYWORD},
    {"struct", KEYWORD},
    {"switch", KEYWORD},
    {"template", KEYWORD},
    {"this", KEYWORD},
    {"throw", KEYWORD},
    {"try", KEYWORD},
    {"typedef", KEYWORD},
    {"typeid", KEYWORD},
    {"typename", KEYWORD},
    {"union", KEYWORD},
    {"using", KEYWORD},
    {"virtual", KEYWORD},
    {"volatile", KEYWORD},
    {"while", KEYWORD},
    {"xor", KEYWORD},
    {"xor_eq", KEYWORD},
    {"true", CONSTANT},
    {"false", CONSTANT},
    {"NULL", CONSTANT},
    {"nullptr", CONSTANT},
}};

} // anonymous namespace

/* Some ugly meta programming used to extract KEYWORDS from words */

template <size_t N>
constexpr uint count_keywords(const array<Word, N>& arr, size_t idx = 0) {
    return (idx == N ? 0 : (arr[idx].style == KEYWORD) + count_keywords(arr, idx + 1));
}

template <size_t N, size_t... Idx>
constexpr array<CStringView, N> extract_keywords_from_append(
    const array<CStringView, N>& base,
    CStringView x,
    std::integer_sequence<size_t, Idx...> /*unused*/
) {
    return {{base[Idx]..., x}};
}

template <size_t N, size_t RES_N, size_t RES_END>
constexpr std::enable_if_t<RES_END >= RES_N, array<CStringView, RES_N>> extract_keywords_from(
    const array<Word, N>& /*unused*/,
    const array<CStringView, RES_N>& res = {},
    size_t /*unused*/ = 0
) {
    return res;
}

template <size_t N, size_t RES_N, size_t RES_END = 0>
    constexpr std::enable_if_t <
    RES_END<RES_N, array<CStringView, RES_N>> extract_keywords_from(
        const array<Word, N>& arr, const array<CStringView, RES_N>& res, size_t idx = 0
    ) {
    return (
        idx == N ? res
                 : (arr[idx].style == KEYWORD
                        ? extract_keywords_from<N, RES_N, RES_END + 1>(
                              arr,
                              extract_keywords_from_append(
                                  res,
                                  {arr[idx].str, arr[idx].size},
                                  std::make_integer_sequence<size_t, RES_END>{}
                              ),
                              idx + 1
                          )
                        : extract_keywords_from<N, RES_N, RES_END>(arr, res, idx + 1))
    );
}

static constexpr auto cpp_keywords =
    extract_keywords_from<words.size(), count_keywords(words)>(words, {});

// Important: elements have to be sorted!
static_assert(
    is_sorted(cpp_keywords),
    "cpp_keywords has to be sorted, fields in words with style "
    "KEYWORD are probably unsorted"
);

namespace sim {

CppSyntaxHighlighter::CppSyntaxHighlighter() {
    static_assert(
        words[0].size == 0,
        "First (zero) element of words is a guard - because "
        "Aho-Corasick implementation takes only positive IDs"
    );
    for (uint i = 1; i < words.size(); ++i) {
        aho.add_pattern({words[i].str, words[i].size}, i);
    }

    aho.build_fail_edges();
}

string CppSyntaxHighlighter::operator()(CStringView input) const {
    string filtered_input; // Used only in the below if
    // Remove (stupid) windows newlines as they impede parsing a lot
    if (input.find("\r\n") != CStringView::npos) {
        filtered_input.reserve(input.size());
        for (size_t i = 0; i < input.size(); ++i) {
            // i + 1 is safe - std::string adds extra '\0' at the end
            if (input[i] == '\r' and input[i + 1] == '\n') {
                ++i;
            }
            filtered_input += input[i];
        }

        input = filtered_input;
    }

    constexpr int BEGIN_GUARDS = 2;
    constexpr int BEGIN = BEGIN_GUARDS;
    constexpr int END_GUARDS = 2;
    constexpr char GUARD_CHARACTER = '\0';

    // Make sure we can use int
    if (input.size() + BEGIN_GUARDS + END_GUARDS >
        static_cast<size_t>(std::numeric_limits<int>::max()))
    {
        THROW("Input string is too long");
    }

    /* Remove "\\\n" sequences */
    int end = input.size();
    // BEGIN_GUARDS x '\0' - guards
    string str(BEGIN_GUARDS, GUARD_CHARACTER); // input without "\\\n" sequences
    str.reserve(end + 64); // Pre-allocation

    for (int i = 0; i < end; ++i) {
        // i + 1 is safe - std::string adds extra '\0' at the end
        if (input[i] == '\\' && input[i + 1] == '\n') {
            ++i;
            continue;
        }

        str += input[i];
    }

    end = str.size();

    // Append end guards (one is already in str - terminating null character)
    str.insert(str.end(), std::max(0, END_GUARDS - 1), GUARD_CHARACTER);
    DEBUG_CSH(stdlog("end: ", end);)

    vector<StyleType> begs(str.size(),
                           -1); // here beginnings of styles are marked
    vector<int8_t> ends(str.size() + 1); // ends[i] = # of endings (of styles) JUST BEFORE str[i]

    /* Mark comments string / character literals */

    for (int i = BEGIN; i < end; ++i) {
        // Comments
        if (str[i] == '/') {
            if (str[i + 1] == '/') { // (One-)line comment
                begs[i] = COMMENT;
                i += 2;
                while (i < end && str[i] != '\n') {
                    ++i;
                }
                ++ends[i];

            } else if (str[i + 1] == '*') { // Multi-line comment
                begs[i] = COMMENT;
                i += 3;
                while (i < end && !(str[i - 1] == '*' && str[i] == '/')) {
                    ++i;
                }
                ++ends[i + 1];
            }
            continue;
        }

        // String / character literals
        if (str[i] == '"' || str[i] == '\'') {
            char literal_delim = str[i];
            begs[i++] = (literal_delim == '"' ? STRING_LITERAL : CHARACTER);
            // End on newline - to mitigate invalid literals
            while (i < end && str[i] != literal_delim && str[i] != '\n') {
                // Ordinary character
                if (str[i] != '\\') {
                    ++i;
                    continue;
                }

                /* Escape sequence */
                begs[i++] = ESCAPED_CHARACTER;

                if (is_digit(str[i])) { // Octals
                    i += (is_digit(str[i + 1]) ? 1 + bool(is_digit(str[i + 2])) : 0);
                } else if (str[i] == 'x') { // Hexadecimal
                    i += 2;
                } else if (str[i] == 'u') { // Unicode U+nnnn
                    i += 4;
                } else if (str[i] == 'U') { // Unicode U+nnnnnnnn
                    i += 8;
                }

                // i has to point to the last character of the escaped sequence
                i = std::min(i + 1, end);
                ++ends[i];
            }
            ++ends[i + 1];
        }
    }

    DEBUG_CSH(auto dump_begs_ends = [&] {
        // i iterates over str, j iterates over input
        for (int i = BEGIN, j = 0, line = 1; i < end; ++i, ++j) {
            while (str[i] != input[j]) {
                throw_assert(input[j] == '\\' && j + 1 < (int)input.size() && input[j + 1] == '\n');
                j += 2;
                ++line;
            }
            auto tmplog = stdlog(i, " (line ", line, "): '");
            if (is_print(str[i])) {
                tmplog(str[i], "'   ");
            } else if (str[i] == '\n') {
                tmplog("\\n'  ");
            } else if (str[i] == '\t') {
                tmplog("\\t'  ");
            } else if (str[i] == '\r') {
                tmplog("\\r'  ");
            } else {
                tmplog("\\x", to_hex({&str[i], 1}), '\'');
            }
            tmplog(" -> ");

            if (ends[i] == 0) {
                tmplog("0, ");
            } else {
                tmplog("\033[1;32m", (int)ends[i], "\033[m, ");
            }

            switch (begs[i]) {
            case -1: tmplog("-1"); break;
            case PREPROCESSOR: tmplog("PREPROCESSOR"); break;
            case COMMENT: tmplog("COMMENT"); break;
            case BUILTIN_TYPE: tmplog("BUILTIN_TYPE"); break;
            case KEYWORD: tmplog("KEYWORD"); break;
            case STRING_LITERAL: tmplog("STRING_LITERAL"); break;
            case CHARACTER: tmplog("CHARACTER"); break;
            case ESCAPED_CHARACTER: tmplog("ESCAPED_CHARACTER"); break;
            case DIGIT: tmplog("DIGIT"); break;
            case CONSTANT: tmplog("CONSTANT"); break;
            case FUNCTION: tmplog("FUNCTION"); break;
            case OPERATOR: tmplog("OPERATOR"); break;
            default: tmplog("???");
            }

            if (str[i] == '\n') {
                ++line;
            }
        }
    };)

    DEBUG_CSH(dump_begs_ends();)

    auto is_name = [](auto c) { return (c == '_' || c == '$' || is_alnum(c)); };

    /* Mark preprocessor, functions and numbers */

    for (int i = BEGIN, styles_depth = 0; i < end; ++i) {
        auto control_style_depth = [&] { styles_depth += (begs[i] != -1) - ends[i]; };

        control_style_depth();
        if (styles_depth > 0) {
            continue;
        }

        if (str[i] == '#') { // Preprocessor
            begs[i] = PREPROCESSOR;
            do {
                ++i;
                control_style_depth();
            } while (i < end && (styles_depth || str[i] != '\n'));
            //                   ^ this is here because preprocessor directives
            // can break (on '\n') with e.g. multi-line comments

            ++ends[i];

        } else if (str[i] == '(' && i) { // Function
            int k = i - 1;
            // Omit white-spaces between function name and '('
            static_assert(BEGIN_GUARDS > 0, "Need for unguarded search");
            while (is_space(str[k])) {
                --k;
            }

            int name_end = k + 1;

            // Function name
            for (;;) {
                // Namespaces
                static_assert(BEGIN_GUARDS > 0);
                if (str[k] == ':' && str[k - 1] == ':' && str[k + 1] != ':') {
                    --k;
                } else if (!is_name(str[k])) {
                    break;
                }

                --k;
            }
            ++k;

            // Function name can neither be blank nor begin with a digit
            if (k < name_end && !is_digit(str[k])) {
                StringView function_name = substring(str, k, name_end);
                // It is important that function_name is taken as StringView
                // below!
                begs[k] =
                    (std::binary_search(cpp_keywords.begin(), cpp_keywords.end(), function_name)
                         ? KEYWORD
                         : FUNCTION);
                ++ends[name_end];
            }

        } else if (is_digit(str[i])) { // Digits - numeric literals
            static_assert(BEGIN_GUARDS > 0);
            // Ignore digits in names
            if (is_name(str[i - 1])) {
                continue;
            }
            // Floating points may begin with '.', but inhibit highlighting
            // the whole number 111.2.3
            static_assert(GUARD_CHARACTER != '.');
            if (str[i - 1] == '.' && is_digit(str[i - 2])) {
                continue;
            }

            // Number begins with dot
            bool dot_appeared = false;
            bool dot_as_beginning = false;
            bool exponent_appeared = false;
            if (str[i - 1] == '.') {
                dot_as_beginning = dot_appeared = true;
                begs[i - 1] = DIGIT;
            } else {
                begs[i] = DIGIT;
            }

            int k = i + 1;
            auto curr_is_digit = is_digit<char>;
            char exp_sign = 'e';

            // Hexadecimal
            if (str[i] == '0' && to_lower(str[k]) == 'x' && !dot_appeared) {
                curr_is_digit = is_xdigit<char>;
                exp_sign = 'p';
                ++k;
                if (str[k] == '.') {
                    dot_as_beginning = dot_appeared = true;
                    ++k;

                } else if (!curr_is_digit(str[k])) { // Disallow numeric literal "0x"
                    begs[i - dot_as_beginning] = -1;
                    i = k - 1;
                    continue;
                }
            }

            // Now the main part of the numeric literal (digits + exponent)
            for (; k < end; ++k) {
                // Digits are always valid
                if (curr_is_digit(str[k])) {
                    continue;
                }

                if (str[k] == '.') { // Dot
                    // Disallow double dot and double exponent
                    if (dot_appeared || exponent_appeared) {
                        goto kill_try;
                    }

                    dot_appeared = true;

                } else if (to_lower(str[k]) == exp_sign) { // Exponent
                    // Do not allow cases like: 0.1e1e or .e2
                    if (exponent_appeared || (str[k - 1] == '.' && dot_as_beginning)) {
                        goto kill_try;
                    }

                    exponent_appeared = true;

                } else if (str[k - 1] == exp_sign && (str[k] == '-' || str[k] == '+'))
                { // Sign after exponent
                    continue;

                } else {
                    break;
                }
            }

            // Ignore invalid numeric literals
            if ((str[k - 1] == '.' && dot_as_beginning) ||
                // In floating-point hexadecimals exponent has to appear
                (exp_sign == 'p' && dot_appeared && !exponent_appeared) ||
                // Allow literals like: "111."
                (str[k - 1] != '.' && !curr_is_digit(str[k - 1])))
            {
            kill_try:
                // dot_as_beginning does not imply beg[i - 1] in hexadecimals
                begs[i - (dot_as_beginning && exp_sign == 'e')] = -1;
                i = k - 1;
                continue;
            }

            /* Suffixes */
            i = k;
            static_assert(END_GUARDS > 0);
            // Float-point: (f|l)
            if (dot_appeared || exponent_appeared) {
                if (str[i] == 'f' || str[i] == 'F' || str[i] == 'l' || str[i] == 'L') {
                    ++i;
                }
                ++ends[i];
                // Go back by one to prevent omitting the next character
                --i;
                ++styles_depth;
                continue;
            }

            // Integer (l|ll|lu|llu|u|ul|ull) or float-point (f)
            if (str[i] == 'f' || str[i] == 'F') {
                ++i;

            } else if (str[i] == 'l' || str[i] == 'L') { // l
                ++i;
                if (str[i] == 'u' || str[i] == 'U') { // lu
                    ++i;
                } else if (str[i] == str[i - 1]) { // ll | LL
                    // ^ Because lL and Ll suffixes are wrong
                    ++i;
                    if (str[i] == 'u' || str[i] == 'U') { // llu
                        ++i;
                    }
                }
            } else if (str[i] == 'u' || str[i] == 'U') { // u
                ++i;
                if (str[i] == 'l' || str[i] == 'L') { // ul
                    ++i;
                    // Because lL and Ll suffixes are wrong
                    if (str[i] == str[i - 1]) { // ull | uLL
                        ++i;
                    }
                }
            }
            ++ends[i];
            // Go back by one to prevent omitting the next character
            --i;
            ++styles_depth;
        }
    }

    DEBUG_CSH(dump_begs_ends();)

    auto is_operator = [](unsigned char c) {
        static constexpr auto xxx = [] {
            array<bool, 128> res{};
            for (char x : StringView("!%&()*+,-./:;<=>?[\\]^{|}~")) {
                res[x] = true;
            }
            return res;
        }();
        return (c < 128 && xxx[c]);
    };

    /* Highlight non-styled code */

    // Highlights fragments of code not containing: comments, preprocessor,
    // number literals, string literals and character literals
    auto highlight = [&](int beg, int endi) {
        DEBUG_CSH(stdlog("highlight(", beg, ", ", endi, "):");)
        if (beg >= endi) {
            return;
        }

        auto aho_res = aho.search_in(substring(str, beg, endi));
        // Handle last one to eliminate right boundary checks
        for (int i = endi - 1; i >= beg;) {
            int k = aho.pattern_id(aho_res[i - beg]);
            int j = 0;

            static_assert(BEGIN_GUARDS > 0);
            static_assert(END_GUARDS > 0);
            // Found a word
            if (k && !is_name(str[i + 1]) && !is_name(str[j = i - words[k].size])) {
                DEBUG_CSH(stdlog("aho: ", j + 1, ": ", words[k].str);)
                begs[j + 1] = words[k].style;
                ++ends[i + 1];
                i = j;
                continue;
            }
            if (is_operator(str[i])) {
                begs[i] = OPERATOR;
                ++ends[i + 1];
            }
            --i;
        }
    };

    /* Find non-styled segments and tun highlight() on them */
    {
        int last_non_styled = 0;
        for (int i = BEGIN, styles_depth = 0; i < end; ++i) {
            int k = styles_depth;
            if (begs[i] != -1) {
                ++styles_depth;
            }
            styles_depth -= ends[i];

            if (k == 0 && styles_depth > 0) {
                highlight(last_non_styled, i);
                // Needed for proper non-styled interval after this loop
                last_non_styled = end;

            } else if (k > 0 && styles_depth == 0) {
                last_non_styled = i;
            }
        }
        highlight(last_non_styled, end); // Take care of last non-styled piece
    }

    DEBUG_CSH(dump_begs_ends();)

    /* Parse styles and produce result */

    string res = "<table class=\"code-view\">"
                 "<tbody>"
                 "<tr><td id=\"L1\" line=\"1\"></td><td>";
    // Stack of styles (needed to properly break on '\n')
    vector<StyleType> style_stack;
    int first_unescaped = BEGIN;
    // i iterates over str, j iterates over input
    for (int i = BEGIN, j = 0, line = 1; i < end; ++i, ++j) {
        // End styles
        if (ends[i]) {
            append_as_html_escaped(res, substring(str, first_unescaped, i));
            first_unescaped = i;

            // Leave the last (top) style and try to elide it
            while (ends[i] > 1) {
                style_stack.pop_back();
                res += end_style;
                --ends[i];
            }

            if (ends[i]) {
                // Style elision (to compress and simplify output)
                if (style_stack.back() == begs[i]) {
                    begs[i] = -1;
                } else {
                    style_stack.pop_back();
                    res += end_style;
                }
            }
        }

        // Handle erased "\\\n" sequences
        if (str[i] != input[j]) {
            append_as_html_escaped(res, substring(str, first_unescaped, i));
            first_unescaped = i;
            // End styles
            for (int k = style_stack.size(); k > 0; --k) {
                res += end_style;
            }
            // Handle "\\\n"
            do {
                throw_assert(input[j] == '\\' && j + 1 < (int)input.size() && input[j + 1] == '\n');

                ++line;
                // When we were ending styles (somewhere above), there can be
                // an opportunity to elide style OPERATOR (only in the first
                // iteration of this loop) but it's not worth that, as it's a
                // very rare situation and gives little profit at the expense of
                // a not pretty if statement
                back_insert(
                    res,
                    begin_style[OPERATOR],
                    '\\',
                    end_style,
                    "\n</td></tr><tr><td id=\"L",
                    line,
                    "\" line=\"",
                    line,
                    "\"></td><td>"
                );
            } while (str[i] != input[j += 2]);

            // Restore styles
            for (StyleType style : style_stack) {
                res += begin_style[style];
            }
        }

        // Line ending
        if (str[i] == '\n') {
            append_as_html_escaped(res, substring(str, first_unescaped, i));
            first_unescaped = i + 1;
            // End styles
            for (int k = style_stack.size(); k > 0; --k) {
                res += end_style;
            }
            // Break the line
            ++line;
            back_insert(
                res, "\n</td></tr><tr><td id=\"L", line, "\" line=\"", line, "\"></td><td>"
            );
            // Restore styles
            for (StyleType style : style_stack) {
                res += begin_style[style];
            }
        }

        // Begin style
        if (begs[i] != -1) {
            append_as_html_escaped(res, substring(str, first_unescaped, i));
            first_unescaped = i;

            style_stack.emplace_back(begs[i]);
            res += begin_style[begs[i]];
        }
    }

    // Ending
    append_as_html_escaped(res, substring(str, first_unescaped, end));
    // End styles
    for (uint i = end; i < ends.size(); ++i) {
        while (ends[i]--) {
            res += end_style;
        }
    }
    // End table
    res += "</td></tr></tbody></table>";

    return res;
}

} // namespace sim

#include "../include/config_file.h"
#include "../include/utilities.h"

#include <gtest/gtest.h>

using std::endl;
using std::pair;
using std::array;
using std::string;
using std::vector;

TEST (ConfigFile, is_string_literal) {
	// (input, output)
	vector<pair<string, bool>> cases {
		{"", false},
		{R"===(foo-bar)===", true},
		{"line: 1\nab d E\n", false},
		{R"===(\\\\\\)===", true},
		{R"===( foo)===", false},
		{R"===(foo )===", false},
		{R"===( foo )===", false},
		{"\n", false},
		{R"===('')===", false},
		{R"===(this isn't a single-quoted string)===", true},
		{R"===(a'a)===", true},
		{R"===("a a")===", false},
		{R"===("a'a")===", false},

		{" ", false},
		{"\t", false},
		{"\r", false},
		{"\n", false},
		{"[", false},
		{",", false},
		{"#", false},
		{"'", false},
		{"\"", false},
		// Beginning
		{" aaaa", false},
		{"\taaaa", false},
		{"\raaaa", false},
		{"\naaaa", false},
		{"[aaaa", false},
		{"]aaaa", false},
		{",aaaa", false},
		{"#aaaa", false},
		{"'aaaa", false},
		{"\"aaaa", false},
		// Ending
		{"aaaa ", false},
		{"aaaa\t", false},
		{"aaaa\r", false},
		{"aaaa\n", false},
		{"aaaa]", false},
		{"aaaa,", false},
		// Ending
		{"aaaa[", true},
		{"aaaa'", true},
		{"aaaa\"", true},
		// Interior
		{"aa\naa", false},
		{"aa]aa", false},
		{"aa,aa", false},
		// Comment
		{"aaaa#", false},
		{"aa#aa", false},
		{"aaaa #", false},
		{"aa #aa", false},
		{"#aaaa", false},
		{"aaaa# ", false},
		{"aaaa # ", false},
		{" #aa", false},
	};

	// Hardcoded tests
	for (auto&& p : cases)
		EXPECT_EQ(ConfigFile::is_string_literal(p.first), p.second)
			<< "p.first: " << p.first << endl;

	auto is_beginning = [](char c) {
		return !(isspace(c) || isOneOf(c, '[', ',', ']', '#', '\'', '"'));
	};
	auto is_interior = [](char c) {
		return !isOneOf(c, '\n', '#', ']', ',');
	};
	auto is_ending = [](char c) {
		return !(isspace(c) || isOneOf(c, '#', ']', ','));
	};
	auto dump = [](int a, int b = -1, int c = -1) {
		char t[3] = {(char)a, (char)b, (char)c};
		return concat_tostr("(a, b, c): ", a, ' ', b, ' ', c, "\n  -> \"",
			StringView(t, 3));
	};

	// Generated tests (all words of length not greater than 3)
	array<char, 4> t;
	for (int a = t[0] = 0; a < 256; t[0] = ++a) {
		// One character
		EXPECT_EQ(ConfigFile::is_string_literal({t.data(), 1}),
				is_beginning(t[0]) && is_ending(t[0]))
			<< dump(a) << endl;

		for (int b = t[1] = 0; b < 256; t[1] = ++b) {
			// Two characters
			EXPECT_EQ(ConfigFile::is_string_literal({t.data(), 2}),
					is_beginning(t[0]) && is_ending(t[1]))
				<< dump(a, b) << endl;

			bool cached_res = is_beginning(t[0]) && is_interior(t[1]);
			for (int c = t[2] = 0; c < 256; t[2] = ++c) {
				// Three characters
				if (ConfigFile::is_string_literal({t.data(), 3}) !=
					cached_res && is_ending(t[2]))
				{
					EXPECT_EQ(ConfigFile::is_string_literal({t.data(), 3}),
							is_beginning(t[0]) && is_interior(t[1]) &&
							is_ending(t[2]))
						<< dump(a, b, c) << endl;
				}
			}
		}
	}

	// Special test cases: ". #." <-- such a sequence cannot appear in a string
	for (int a = 0; a < 256; ++a)
		for (int b = 0; b < 256; ++b) {
			t = {{(char)a, ' ', '#', (char)b}};
			EXPECT_EQ(ConfigFile::is_string_literal(StringView(t.data(), 4)),
					false)
				<< "a: " << a << " b: " << b << " str: "
				<< StringView(t.data(), 4) << endl;
		}

}

TEST (ConfigFile, escape_to_single_quoted_string) {
	// (input, output)
	vector<pair<string, string>> cases {
		{"", "''"},
		{R"===(foo-bar)===", R"===('foo-bar')==="},
		{"line: 1\nab d E\n", "'line: 1\nab d E\n'"},
		{R"===(\\\\\\)===", R"===('\\\\\\')==="},
		{R"===( foo)===", R"===(' foo')==="},
		{R"===(foo )===", R"===('foo ')==="},
		{R"===( foo )===", R"===(' foo ')==="},
		{R"===('')===", R"===('''''')==="},
		{R"===(this isn't a single-quoted string)===",
			R"===('this isn''t a single-quoted string')==="},
		{R"===(a'a)===", R"===('a''a')==="},
		{R"===("a a")===", R"===('"a a"')==="},
		{R"===("a'a")===", R"===('"a''a"')==="},

		{" ", "' '"},
		{"\t", "'\t'"},
		{"\r", "'\r'"},
		{"\n", "'\n'"},
		{"[", "'['"},
		{",", "','"},
		{"#", "'#'"},
		{"'", "''''"},
		{"\"", "'\"'"},
		// Beginning
		{" aaaa", "' aaaa'"},
		{"\taaaa", "'\taaaa'"},
		{"\raaaa", "'\raaaa'"},
		{"\naaaa", "'\naaaa'"},
		{"[aaaa", "'[aaaa'"},
		{"]aaaa", "']aaaa'"},
		{",aaaa", "',aaaa'"},
		{"#aaaa", "'#aaaa'"},
		{"'aaaa", "'''aaaa'"},
		{"\"aaaa", "'\"aaaa'"},
		// Ending
		{"aaaa ", "'aaaa '"},
		{"aaaa\t", "'aaaa\t'"},
		{"aaaa\r", "'aaaa\r'"},
		{"aaaa\n", "'aaaa\n'"},
		{"aaaa]", "'aaaa]'"},
		{"aaaa,", "'aaaa,'"},
		// Ending
		{"aaaa[", "'aaaa['"},
		{"aaaa'", "'aaaa'''"},
		{"aaaa\"", "'aaaa\"'"},
		// Interior
		{"aa\naa", "'aa\naa'"},
		{"aa]aa", "'aa]aa'"},
		{"aa,aa", "'aa,aa'"},
		// Comment
		{"aaaa#", "'aaaa#'"},
		{"aa#aa", "'aa#aa'"},
		{"aaaa #", "'aaaa #'"},
		{"aa #aa", "'aa #aa'"},
		{"#aaaa", "'#aaaa'"},
		{"aaaa# ", "'aaaa# '"},
		{"aaaa # ", "'aaaa # '"},
		{" #aa", "' #aa'"},
	};

	for (auto&& p : cases)
		EXPECT_EQ(ConfigFile::escape_to_single_quoted_string(p.first), p.second)
			<< "p.first: " << p.first << endl;
}

TEST (ConfigFile, escape_to_double_quoted_string) {
	// (input, output)
	vector<pair<string, string>> cases {
		{"", R"===("")==="},
		{R"===(foo-bar)===", R"===("foo-bar")==="},
		{"line: 1\nab d E\n", R"===("line: 1\nab d E\n")==="},
		{R"===(\\\\\\)===", R"===("\\\\\\\\\\\\")==="},
		{R"===( foo)===", R"===(" foo")==="},
		{R"===(foo )===", R"===("foo ")==="},
		{R"===( foo )===", R"===(" foo ")==="},
		{R"===('')===", R"===("''")==="},
		{R"===(this isn't a single-quoted string)===",
			R"===("this isn't a single-quoted string")==="},
		{R"===(a'a)===", R"===("a'a")==="},
		{R"===("a a")===", R"===("\"a a\"")==="},
		{R"===("a'a")===", R"===("\"a'a\"")==="},
		{" ", R"===(" ")==="},
		{"\t", R"===("\t")==="},
		{"\r", R"===("\r")==="},
		{"\n", R"===("\n")==="},
		{"[", R"===("[")==="},
		{",", R"===(",")==="},
		{"#", R"===("#")==="},
		{"'", R"===("'")==="},
		{"\"", R"===("\"")==="},
		// Beginning
		{" aaaa", R"===(" aaaa")==="},
		{"\taaaa", R"===("\taaaa")==="},
		{"\raaaa", R"===("\raaaa")==="},
		{"\naaaa", R"===("\naaaa")==="},
		{"[aaaa", R"===("[aaaa")==="},
		{"]aaaa", R"===("]aaaa")==="},
		{",aaaa", R"===(",aaaa")==="},
		{"#aaaa", R"===("#aaaa")==="},
		{"'aaaa", R"===("'aaaa")==="},
		{"\"aaaa", R"===("\"aaaa")==="},
		// Ending
		{"aaaa ", R"===("aaaa ")==="},
		{"aaaa\t", R"===("aaaa\t")==="},
		{"aaaa\r", R"===("aaaa\r")==="},
		{"aaaa\n", R"===("aaaa\n")==="},
		{"aaaa]", R"===("aaaa]")==="},
		{"aaaa,", R"===("aaaa,")==="},
		// Ending
		{"aaaa[", R"===("aaaa[")==="},
		{"aaaa'", R"===("aaaa'")==="},
		{"aaaa\"", R"===("aaaa\"")==="},
		// Interior
		{"aa\naa", R"===("aa\naa")==="},
		{"aa]aa", R"===("aa]aa")==="},
		{"aa,aa", R"===("aa,aa")==="},
		// Comment
		{"aaaa#", R"===("aaaa#")==="},
		{"aa#aa", R"===("aa#aa")==="},
		{"aaaa #", R"===("aaaa #")==="},
		{"aa #aa", R"===("aa #aa")==="},
		{"#aaaa", R"===("#aaaa")==="},
		{"aaaa# ", R"===("aaaa# ")==="},
		{"aaaa # ", R"===("aaaa # ")==="},
		{" #aa", R"===(" #aa")==="},
	};

	for (auto&& p : cases) {
		// First version
		EXPECT_EQ(ConfigFile::escape_to_double_quoted_string(p.first), p.second)
			<< "p.first: " << p.first << endl;
		// Second version
		EXPECT_EQ(ConfigFile::escape_to_double_quoted_string(p.first, 0), p.second)
			<< "p.first: " << p.first << endl;
	}

	// Escaping control characters
	EXPECT_EQ(ConfigFile::escape_to_double_quoted_string("\x07\x0e\n\x15\x1f"),
		R"===("\a\x0e\n\x15\x1f")===");
	EXPECT_EQ(ConfigFile::escape_to_double_quoted_string("\x02\x03\tśćąłź\x11"),
		R"===("\x02\x03\tśćąłź\x11")===");
	EXPECT_EQ(ConfigFile::escape_to_double_quoted_string("ś"), R"===("ś")===");
	EXPECT_EQ(ConfigFile::escape_to_double_quoted_string(" '\t\n' ś "),
		R"===(" '\t\n' ś ")===");
	// Escaping unprintable characters
	EXPECT_EQ(ConfigFile::escape_to_double_quoted_string("\x07\x0e\n\x15\x1f", 0),
		R"===("\a\x0e\n\x15\x1f")===");
	EXPECT_EQ(ConfigFile::escape_to_double_quoted_string("\x02\x03\tśćąłź\x11", 0),
		R"===("\x02\x03\t\xc5\x9b\xc4\x87\xc4\x85\xc5\x82\xc5\xba\x11")===");
	EXPECT_EQ(ConfigFile::escape_to_double_quoted_string("ś", 0),
		R"===("\xc5\x9b")===");
	EXPECT_EQ(ConfigFile::escape_to_double_quoted_string(" '\t\n' ś ", 0),
		R"===(" '\t\n' \xc5\x9b ")===");
}

TEST (ConfigFile, escape_string) {
	// (input, output)
	vector<pair<string, string>> cases {
		{"", "''"},
		{R"===(foo-bar)===", R"===(foo-bar)==="},
		{"line: 1\nab d E\n", R"===("line: 1\nab d E\n")==="},
		{R"===(\\\\\\)===", R"===(\\\\\\)==="},
		{R"===( foo)===", R"===(' foo')==="},
		{R"===(foo )===", R"===('foo ')==="},
		{R"===( foo )===", R"===(' foo ')==="},
		{R"===('')===", R"===("''")==="},
		{R"===(this isn't a single-quoted string)===",
			R"===("this isn't a single-quoted string")==="},
		{R"===(a'a)===", R"===("a'a")==="},
		{R"===("a a")===", R"===('"a a"')==="},
		{R"===("a'a")===", R"===("\"a'a\"")==="},
		{" ", R"===(' ')==="},
		{"\t", R"===("\t")==="},
		{"\r", R"===("\r")==="},
		{"\n", R"===("\n")==="},
		{"[", R"===('[')==="},
		{",", R"===(',')==="},
		{"#", R"===('#')==="},
		{"'", R"===("'")==="},
		{"\"", R"===('"')==="},
		// Beginning
		{" aaaa", R"===(' aaaa')==="},
		{"\taaaa", R"===("\taaaa")==="},
		{"\raaaa", R"===("\raaaa")==="},
		{"\naaaa", R"===("\naaaa")==="},
		{"[aaaa", R"===('[aaaa')==="},
		{"]aaaa", R"===(']aaaa')==="},
		{",aaaa", R"===(',aaaa')==="},
		{"#aaaa", R"===('#aaaa')==="},
		{"'aaaa", R"===("'aaaa")==="},
		{"\"aaaa", R"===('"aaaa')==="},
		// Ending
		{"aaaa ", R"===('aaaa ')==="},
		{"aaaa\t", R"===("aaaa\t")==="},
		{"aaaa\r", R"===("aaaa\r")==="},
		{"aaaa\n", R"===("aaaa\n")==="},
		{"aaaa]", R"===('aaaa]')==="},
		{"aaaa,", R"===('aaaa,')==="},
		// Ending
		{"aaaa[", R"===(aaaa[)==="},
		{"aaaa'", R"===("aaaa'")==="},
		{"aaaa\"", R"===(aaaa")==="},
		// Interior
		{"aa\naa", R"===("aa\naa")==="},
		{"aa]aa", R"===('aa]aa')==="},
		{"aa,aa", R"===('aa,aa')==="},
		// Comment
		{"aaaa#", R"===('aaaa#')==="},
		{"aa#aa", R"===('aa#aa')==="},
		{"aaaa #", R"===('aaaa #')==="},
		{"aa #aa", R"===('aa #aa')==="},
		{"#aaaa", R"===('#aaaa')==="},
		{"aaaa# ", R"===('aaaa# ')==="},
		{"aaaa # ", R"===('aaaa # ')==="},
		{" #aa", R"===(' #aa')==="},
	};

	for (auto&& p : cases) {
		// First version
		EXPECT_EQ(ConfigFile::escape_string(p.first), p.second)
			<< "p.first: " << p.first << endl;
		// Second version
		EXPECT_EQ(ConfigFile::escape_string(p.first, 0), p.second)
			<< "p.first: " << p.first << endl;
	}

	// Escaping control characters
	EXPECT_EQ(ConfigFile::escape_string("\x07\x0e\n\x15\x1f"),
		R"===("\a\x0e\n\x15\x1f")===");
	EXPECT_EQ(ConfigFile::escape_string("\x02\x03\tśćąłź\x11"),
		R"===("\x02\x03\tśćąłź\x11")===");

	EXPECT_EQ(ConfigFile::escape_string("ś"), R"===(ś)===");
	EXPECT_EQ(ConfigFile::escape_string(" '\t\n' ś "), R"===(" '\t\n' ś ")===");

	// Escaping unprintable characters
	EXPECT_EQ(ConfigFile::escape_string("\x07\x0e\n\x15\x1f", 0),
		R"===("\a\x0e\n\x15\x1f")===");
	EXPECT_EQ(ConfigFile::escape_string("\x02\x03\tśćąłź\x11", 0),
		R"===("\x02\x03\t\xc5\x9b\xc4\x87\xc4\x85\xc5\x82\xc5\xba\x11")===");

	EXPECT_EQ(ConfigFile::escape_string("ś", 0), R"===("\xc5\x9b")===");
	EXPECT_EQ(ConfigFile::escape_string(" '\t\n' ś ", 0),
		R"===(" '\t\n' \xc5\x9b ")===");
}

string dumpConfig(const ConfigFile& cf) {
	auto&& vars = cf.get_vars();
	string res;
	for (auto p : vars) {
		// Array
		if (p.second.is_array()) {
			back_insert(res, p.first, ": [\n");
			for (auto&& s : p.second.as_array())
				back_insert(res, "  ",
					ConfigFile::escape_to_double_quoted_string(s, 0), '\n');
			res += ']';
		// Other
		} else {
			back_insert(res, p.first, ": ",
				ConfigFile::escape_to_double_quoted_string(p.second.as_string(), 0));
		}

		// Include the value position
		back_insert(res, " # [", p.second.value_span.beg, ",",
			p.second.value_span.end, ")\n");
	};
	return res;
}

TEST (ConfigFile, load_config_from_string) {
	// (input, dumped config)
	vector<pair<string, string>> cases {
		/*  1 */ {R"===()===", ""},
		/*  2 */ {"    a  = 1234", "a: \"1234\" # [9,13)\n"},
		/*  3 */ {" a : eee \t\n ", "a: \"eee\" # [5,8)\n"},
		/*  4 */ {"a=:eee\t\n ", "a: \":eee\" # [2,6)\n"},
		/*  5 */ {"a = abą'\n ", "a: \"ab\\xc4\\x85'\" # [4,9)\n"},
		/*  6 */ {"a: aa'\t\n ", "a: \"aa'\" # [3,6)\n"},
		/*  7 */ {"a: aa\"\t\n ", "a: \"aa\\\"\" # [3,6)\n"},
		/*  8 */ {"a: aa\"#aaa\n ", "a: \"aa\\\"\" # [3,6)\n"},
		/*  9 */ {"a: aa\" #aaa\n ", "a: \"aa\\\"\" # [3,6)\n"},
		/* 10 */ {"a: aa\" # aaa\n ", "a: \"aa\\\"\" # [3,6)\n"},
		/* 11 */ {"a: '\"It''s awesome\"'\n ", "a: \"\\\"It's awesome\\\"\" # [3,20)\n"},
		/* 12 */ {"a = \"abą\"\n ", "a: \"ab\\xc4\\x85\" # [4,10)\n"},
		/* 13 */ {"a = \"ab\\xa61\"\n ", "a: \"ab\\xa61\" # [4,13)\n"},
		/* 14 */ {"a = b]b", "a: \"b]b\" # [4,7)\n"},
		/* 15 */ {"a = b,b", "a: \"b,b\" # [4,7)\n"},
		/* 16 */ {"a = bb]", "a: \"bb]\" # [4,7)\n"},
		/* 17 */ {"a = bb,", "a: \"bb,\" # [4,7)\n"},
		/* 18 */ {"a = ,a", "a: \",a\" # [4,6)\n"},
		/* 19 */ {"a = ]a", "a: \"]a\" # [4,6)\n"},
		/* 20 */ {"a = a\nb = ,]", "a: \"a\" # [4,5)\nb: \",]\" # [10,12)\n"},
		/* 21 */ {R"===(# Server address
address: 127.7.7.7:8080

# Number of server workers (cannot be lower than 1)
workers: 2
# Number of connections (cannot be lower than 1)
connections= 100 # '=' may also be used as an assignment operator

# In case of normal variables (not arrays) value may be omitted
empty-var.try_it: # dash, dot and underscore are allowed in variable name


#Comments without trailing white-space also works
foo=bar#Comments may appear on the end of the variable (also in arrays)

# Values may be also single-quoted strings
a1 = ' test test '
# in which an apostrophe may be escaped with sequence '' for example:
a2 = 'This''s awesome'

# Double quoted strings also may be used
b1 = "\n\n'Foo bar'\n\ttest\n # This is not a comment" # But this IS a comment

test1 = [an, inline, array]
test2 = [
  # In arrays empty (not containing a value) lines are ignored
  '1',
  2,
  3 # Instead of a comma newline may also be used as a delimiter
    # But be careful, because many newlines and commas (delimiters) between two
    # values are treated as one. At the beginning and the ending of an array
    # delimiters are also ignored.
  4, 5

  "6", '7', 8
]

test3 = [,,1,2,,3,
  4
  ,5
  ,
]
# test3 and test4 are equivalent
test4 = [1, 2, 3, 4, 5]

test5 = [1, 2
  3
  4
  5 6 7 # This is equivalent to '5 6 7'
]
)===", R"===(a1: " test test " # [532,545)
a2: "This's awesome" # [621,638)
address: "127.7.7.7:8080" # [26,40)
b1: "\n\n'Foo bar'\n\ttest\n # This is not a comment" # [686,735)
connections: "100" # [167,170)
empty-var.try_it: "" # [303,303)
foo: "bar" # [415,418)
test1: [
  "an"
  "inline"
  "array"
] # [769,788)
test2: [
  "1"
  "2"
  "3"
  "4"
  "5"
  "6"
  "7"
  "8"
] # [797,1154)
test3: [
  "1"
  "2"
  "3"
  "4"
  "5"
] # [1164,1189)
test4: [
  "1"
  "2"
  "3"
  "4"
  "5"
] # [1231,1246)
test5: [
  "1"
  "2"
  "3"
  "4"
  "5 6 7"
] # [1256,1311)
workers: "2" # [103,104)
)==="},
	};

	for (size_t i = 0; i < cases.size(); ++i) {
		auto&& p = cases[i];

		ConfigFile cf;
		cf.load_config_from_string(p.first, true);
		RecordProperty("Case", i + 1);
		EXPECT_EQ(dumpConfig(cf), p.second) << "Case: " << i + 1 << endl;
	}

	ConfigFile cf; // It do not have to be cleaned - we watch only for
	               // exceptions
	// TODO: if / when EXPECT_THROW_WHAT will be available in Google Test,
	// use it here
	EXPECT_THROW(cf.load_config_from_string("a & eee "), ConfigFile::ParseError);
	EXPECT_THROW(cf.load_config_from_string("a = [a"), ConfigFile::ParseError);
	EXPECT_THROW(cf.load_config_from_string("a = 'a"), ConfigFile::ParseError);
	EXPECT_THROW(cf.load_config_from_string("a = \"a"), ConfigFile::ParseError);
	EXPECT_THROW(cf.load_config_from_string("a = b\nb"), ConfigFile::ParseError);
	EXPECT_THROW(cf.load_config_from_string("a = \"b\nb\""), ConfigFile::ParseError);
	EXPECT_THROW(cf.load_config_from_string("a = 'b\nb'"), ConfigFile::ParseError);
	EXPECT_THROW(cf.load_config_from_string("a = 'b'b'"), ConfigFile::ParseError);
	EXPECT_THROW(cf.load_config_from_string("a = \"b\"b\""), ConfigFile::ParseError);
	EXPECT_THROW(cf.load_config_from_string("a = \"\\q\""), ConfigFile::ParseError);
	EXPECT_THROW(cf.load_config_from_string("a = a\n\n&" ), ConfigFile::ParseError);
	EXPECT_THROW(cf.load_config_from_string("a = a\n\na = '" ), ConfigFile::ParseError);
	EXPECT_THROW(cf.load_config_from_string("a = \"\\xg21\""), ConfigFile::ParseError);
	EXPECT_THROW(cf.load_config_from_string("a = \"\\x2g1\""), ConfigFile::ParseError);
	EXPECT_THROW(cf.load_config_from_string("a = \"\\x\""), ConfigFile::ParseError);
	EXPECT_THROW(cf.load_config_from_string("a = \"\\xa\""), ConfigFile::ParseError);
	EXPECT_THROW(cf.load_config_from_string(";"), ConfigFile::ParseError);
}

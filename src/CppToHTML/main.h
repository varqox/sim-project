#pragma once

#include <iostream>
#include <fstream>
#include <cstring>

using namespace std;

typedef const char* const_string;

namespace span
{
	const const_string
	preprocessor("<span style=\"color: #00a000\">"),
	comment("<span style=\"color: #a0a0a0\">"),
	string("<span style=\"color: #ff0000\">"),
	character("<span style=\"color: #e0a000\">"),
	special_character("<span style=\"color: #d923e9\">"),
	number("<span style=\"color: #d923e9\">"),
	keyword("<span style=\"color: #0000ff;font-weight: bold\">"),
	type("<span style=\"color: #c90049;font-weight: bold;\">"),
	function("<span style=\"\">"),
	operators("<span style=\"color: #61612d\">"),
	true_false("<span style=\"color: #d923e9\">"),
	end("</span>");
}

// main.cpp
extern const bool is_name[], is_true_name[];

inline string safe_character(char _c)
{
	if(_c=='<') return "&lt;";
	if(_c=='>') return "&gt;";
	if(_c=='&') return "&amp;";
	if(_c=='\r') return "";
	return string(1, _c);
}

inline string safe_string(const string& str)
{
	string ret;
	for(string::const_iterator i=str.begin(); i!=str.end(); ++i)
		ret+=safe_character(*i);
return ret;
}

// coloring.cpp
namespace coloring
{
	void init();
	string synax_highlight(const string& code);
	void color_code(const string& code, ostream& output);
}

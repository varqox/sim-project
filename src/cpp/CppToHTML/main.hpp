#pragma once

#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <queue>

using namespace std;

class const_string
{
	char* _M_str;
public:
	explicit const_string(const char* _str): _M_str(new char[strlen(_str)+1])
	{
		memcpy(_M_str, _str, strlen(_str)+1);
	}
	const_string(const const_string& _cstr): _M_str(_cstr._M_str) {}
	const_string& operator=(const const_string& _cstr)
	{
		_M_str=_cstr._M_str;
	return *this;
	}
	~const_string(){}

	const char* str() const
	{return _M_str;}

	bool operator==(const const_string& _cstr)
	{return this==&_cstr;}

	bool operator!=(const const_string& _cstr)
	{return this!=&_cstr;}

	template<class ostream_type>
	friend ostream_type& operator<<(ostream_type& os, const const_string& _cstr)
	{return os << _cstr._M_str;}

	operator const char*() const
	{return _M_str;}
};

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
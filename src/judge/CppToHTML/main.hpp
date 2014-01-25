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
		memcpy(this->_M_str, _str, strlen(_str)+1);
	}
	const_string(const const_string& _cstr): _M_str(_cstr._M_str) {}
	const_string& operator=(const const_string& _cstr)
	{
		this->_M_str=_cstr._M_str;
	return *this;
	}
	~const_string(){}

	const char* str() const
	{return this->_M_str;}

	bool operator==(const const_string& _cstr)
	{return this==&_cstr;}

	bool operator!=(const const_string& _cstr)
	{return this!=&_cstr;}

	template<class ostream_type>
	friend ostream_type& operator<<(ostream_type& os, const const_string& _cstr)
	{return os << _cstr._M_str;}

	operator const char*() const
	{return this->_M_str;}
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
	if(_c=='<') return "&lt";
	if(_c=='>') return "&gt";
	if(_c=='&') return "&amp";
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

// aho.cpp
class aho
{
	class class_trie
	{
	public:
		struct node
		{
			int E[256], fail, long_sh_pat, pattern_id; // fail pointer, max shorter pattern, pattern id
			bool is_pattern; // is pattern end in this vertex
			unsigned char character; // this node character
			node(unsigned char letter=0): fail(), long_sh_pat(), pattern_id(), is_pattern(false), character(letter)
			{
				for(int i=0; i<256; ++i)
					E[i]=0;
			}
			~node(){}
		};

		vector<node> graph;

		class_trie(): graph(1) // add root
		{
			this->graph.front().fail=this->graph.front().long_sh_pat=0; // max shorter pattern isn't exist
		}

		void swap(class_trie& _t)
		{
			this->graph.swap(_t.graph);
		}

		int add_word(const string& word, int id);
		void add_fails(); // and the longest shorter patterns, based on BFS algorithm
	} trie;

	vector<vector<unsigned>* > fin; // finding patterns

public:
	aho(): trie(), fin(){}
	~aho(){}

	vector<vector<unsigned>* >::size_type size()
	{return this->fin.size();}

	vector<unsigned>& operator[](vector<vector<unsigned>* >::size_type n)
	{return *this->fin[n];}

	const vector<unsigned>& operator[](vector<vector<unsigned>* >::size_type n) const
	{return *this->fin[n];}

	void swap(aho& _a)
	{
		this->trie.swap(_a.trie);
		this->fin.swap(_a.fin);
	}

	void find(const vector<string>& patterns, const string& text);
};

class special_aho
{
	class class_trie
	{
	public:
		struct node
		{
			int E[256], fail, long_sh_pat, pattern_id; // fail pointer, max shorter pattern, pattern id
			bool is_pattern; // is pattern end in this vertex
			// unsigned char color; // highlight color
			unsigned char character; // this node character
			node(unsigned char letter=0): fail(), long_sh_pat(), pattern_id(), is_pattern(false), character(letter)
			{
				for(int i=0; i<256; ++i)
					E[i]=0;
			}
			~node(){}
		};

		vector<node> graph;

		class_trie(): graph(1) // add root
		{
			this->graph.front().fail=this->graph.front().long_sh_pat=0; // max shorter pattern isn't exist
		}

		void swap(class_trie& _t)
		{
			this->graph.swap(_t.graph);
		}

		int add_word(const string& word, int id);
		void add_fails(); // and the longest shorter patterns, based on BFS algorithm
	} trie;

	vector<pair<string, const_string> > patterns;
	vector<int> fin; // finding patterns

public:
	special_aho(): trie(), patterns(), fin(){}
	~special_aho(){}

	vector<int>::size_type size()
	{return this->fin.size();}

	int& operator[](vector<int>::size_type n)
	{return this->fin[n];}

	const int& operator[](vector<int>::size_type n) const
	{return this->fin[n];}

	void swap(special_aho& _a)
	{
		this->trie.swap(_a.trie);
		this->fin.swap(_a.fin);
	}

	const pair<string, const_string>& pattern(vector<pair<string, const_string> >::size_type n) const
	{return this->patterns[n];}

	void set_patterns(const vector<pair<string, const_string> >& new_patterns);

	void find(const string& text);
};
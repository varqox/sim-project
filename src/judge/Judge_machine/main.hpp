#include <string>
#include <deque>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <set>

#pragma once

// main.cpp
extern std::set<std::string> temporary_files;

// other_functions.cpp
std::string myto_string(long long int a);
std::string f_time(int a);
std::string make_safe_php_string(const std::string& str);
std::string make_safe_html_string(const std::string& str);
std::deque<int> kmp(const std::string& text, const std::string& pattern);
std::string file_get_contents(const std::string& file_name);

// compile.cpp
class compile
{
private:
	std::string compile_errors;
	char *file_compile_errors;
	compile(): file_compile_errors(new char[27])
	{
		memcpy(this->file_compile_errors, "/tmp/compile_errors.XXXXXX", sizeof(char)*27);
		mkstemp(this->file_compile_errors);
		temporary_files.insert(this->file_compile_errors);
	}
	compile(const compile&){}
	~compile()
	{
		remove(file_compile_errors);
		temporary_files.erase(this->file_compile_errors);
	}

public:
	static compile run;

	// Report ID, exec_name in chroot/
	bool operator()(const std::string& report_id, const std::string& exec);

	const char* NameOfCompileErrorsFile()
	{return this->file_compile_errors;}

	const std::string& GetCompileErrors() const
	{return this->compile_errors;}
};
// reports_queue.cpp
namespace reports_queue
{
	bool empty();
	std::string extract();
	const std::string& front();
	void pop();
}
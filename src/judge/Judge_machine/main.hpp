#include <string>
#include <deque>

#pragma once

// main.cpp

// other_functions.cpp
std::string myto_string(long long int a);
std::string f_time(int a);
std::string make_safe_php_string(const std::string& str);
std::string make_safe_html_string(const std::string& str);
std::deque<int> kmp(const std::string& text, const std::string& pattern);
std::string file_get_contents(const std::string& file_name);

// compile.cpp
namespace compile
{
	extern std::string compile_errors;

	// Report ID, exec_name in chroot/
	bool run(const std::string& report_id, const std::string& exec);
}

// reports_queue.cpp
namespace reports_queue
{
	bool empty();
	std::string extract();
	const std::string& front();
	void pop();
}
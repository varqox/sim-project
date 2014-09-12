#include <string>
#include <deque>

#pragma once

void remove_r(const char* path);
std::string myto_string(long long int a);
std::string make_safe_php_string(const std::string& str);
std::string make_safe_html_string(const std::string& str);
std::deque<unsigned> kmp(const std::string& text, const std::string& pattern);
std::string file_get_contents(const std::string& file_name);

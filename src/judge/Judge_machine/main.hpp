#include <string>
#include <deque>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h> // chmod()
#include <set>

#pragma once

// other_functions.cpp
void remove_r(const char* path);
std::string myto_string(long long int a);
std::string f_time(int a);
std::string make_safe_php_string(const std::string& str);
std::string make_safe_html_string(const std::string& str);
std::deque<unsigned> kmp(const std::string& text, const std::string& pattern);
std::string file_get_contents(const std::string& file_name);

// main.cpp
class temporary_directory
{
	char* _M_name;
public:
	explicit temporary_directory(const char* new_name): _M_name(new char[strlen(new_name)+2])
	{
		unsigned size=strlen(new_name);
		memcpy(this->_M_name, new_name, size);
		this->_M_name[size]=this->_M_name[size+1]='\0';
		if(NULL==mkdtemp(this->_M_name))
		{
			struct exception : std::exception
			{
				const char* what() const _GLIBCXX_USE_NOEXCEPT {return "Cannot create tmp directory\n";}
			};
			throw exception();
		}
		this->_M_name[size]='/';
		chmod(this->_M_name, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	}

	~temporary_directory()
	{
		remove_r(this->_M_name);
		delete[] this->_M_name;
	}

	const char* name() const
	{return this->_M_name;}

	operator const char*() const
	{return this->_M_name;}
};

extern temporary_directory tmp_dir;

// compile.cpp
class compile
{
private:
	std::string compile_errors, file_compile_errors;
	compile(): file_compile_errors(std::string(tmp_dir)+"compile_errors"){}
	compile(const compile&){}
	~compile()
	{
		remove(file_compile_errors.c_str());;
	}

public:
	static compile run;

	// Report ID, exec_name in chroot/
	bool operator()(const std::string& report_id, const std::string& exec);

	const char* NameOfCompileErrorsFile()
	{return this->file_compile_errors.c_str();}

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
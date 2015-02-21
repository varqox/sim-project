#pragma once

#include "other_functions.h"
#include "submissions_queue.h"
#include <string>
#include <deque>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h> // chmod()
#include <set>
#include <iostream>

#define eprint(...) fprintf(stderr, __VA_ARGS__)

template<class C>
inline std::string& operator<<(const std::string& s, const C& x)
{return const_cast<std::string&>(s) += x;}

#ifdef DEBUG
#define D(...) __VA_ARGS__
#else
#define D(...)
#endif

// main.cpp
class temporary_directory
{
	char* _M_name;
	temporary_directory(const temporary_directory&): _M_name(NULL){}
	temporary_directory& operator=(const temporary_directory&){return *this;}

public:
	explicit temporary_directory(const char* new_name): _M_name(new char[strlen(new_name)+2])
	{
		unsigned size=strlen(new_name);
		memcpy(_M_name, new_name, size);
		_M_name[size]=_M_name[size+1]='\0';
		if(NULL==mkdtemp(_M_name))
		{
			struct exception : std::exception
			{
				const char* what() const throw() {return "Cannot create tmp directory\n";}
			};
			throw exception();
		}
		_M_name[size]='/';
		chmod(_M_name, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	}

	~temporary_directory()
	{
		D(std::cerr << "\033[1;31mRemoving tmp_dir \033[0m\n" << this << std::endl);
		remove_r(_M_name);
		delete[] _M_name;
	}

	const char* name() const
	{return _M_name;}

	operator const char*() const
	{return _M_name;}
};

extern temporary_directory tmp_dir;

class CompileClass {
private:
	std::string errors_;

public:
	CompileClass(): errors_() {}
	int operator()(const std::string& source_file, const std::string& exec_file);
	std::string getErrors() { return errors_; }
};

extern CompileClass compile;

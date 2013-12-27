#include "main.hpp"
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>

class task
{
	std::string _name, outf_name, memory_limit;
	double min_group_ratio;

	std::string check_on_test(const std::string& test, const std::string& time_limit);

public:
	task(const std::string& str): _name(str)
	{
		if(*this->_name.rbegin()!='/') this->_name+='/';
		// get name of temporary file
		char tmp[]="judge_machine.XXXXXX";
		mkstemp(tmp);
		this->outf_name=tmp;
		temporary_files.insert(this->outf_name);
	}

	~task()
	{
		remove(this->outf_name.c_str());
		temporary_files.insert(this->outf_name);
	}

	const std::string& name() const
	{return this->_name;}

	void swap(task& _t)
	{
		this->_name.swap(_t._name);
		this->memory_limit.swap(_t.memory_limit);
	}

	std::string judge();
};
#include "main.hpp"
#include <vector>

#pragma once

class task
{
	std::string _name, outf_name, memory_limit, checker, exec;
	double min_group_ratio;

	std::string check_on_test(const std::string& test, const std::string& time_limit);

public:
	task(const std::string& str): _name(str), outf_name(), memory_limit(), checker(), exec(),  min_group_ratio()
	{
		if(*this->_name.rbegin()!='/') this->_name+='/';
		this->outf_name=std::string(tmp_dir)+"exec_out";
	}

	~task()
	{
		remove(this->outf_name.c_str());
	}

	const std::string& name() const
	{return this->_name;}

	void swap(task& _t)
	{
		this->_name.swap(_t._name);
	}

	std::string judge(const std::string& exec_name);
};
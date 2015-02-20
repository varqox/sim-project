#include "main.h"
#include <vector>

#pragma once

class Problem
{
public:
	struct JudgeResult {
		struct Group {
			std::string tests, comments;

			Group(): tests(), comments() {}
		} initial, final;

		JudgeResult(): initial(), final() {}
	};

private:
	std::string _name, outf_name, checker, exec;
	double min_group_ratio;

	JudgeResult::Group check_on_test(const std::string& test, const std::string& time_limit, long long mem_limit);

public:
	Problem(const std::string& str): _name(str), outf_name(), checker(), exec(), min_group_ratio()
	{
		if(*_name.rbegin()!='/') _name+='/';
		outf_name=std::string(tmp_dir)+"exec_out";
	}

	~Problem()
	{
		remove(outf_name.c_str());
	}

	const std::string& name() const
	{return _name;}

	void swap(Problem& _t)
	{
		_name.swap(_t._name);
	}

	JudgeResult judge(const std::string& exec_name);
};

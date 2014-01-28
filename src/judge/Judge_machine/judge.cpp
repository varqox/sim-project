#include "judge.hpp"
#include <sys/time.h>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <cstdio>
#include <fstream>
#include <deque>

using namespace std;

string f_time(int a)
{
	string w;
	while(a>0)
	{
		w=static_cast<char>(a%10+'0')+w;
		a/=10;
	}
	if(w.size()<3) w.insert(0,"000",3-w.size());
	w.insert(w.size()-2, ".");
return w;
}

string task::check_on_test(const string& test, const string& time_limit)
{
	string command="ulimit -v "+memory_limit+"; timeout "+time_limit+" chroot --userspec=1001 chroot/ ./"+exec+" < "+_name+"tests/"+test+".in > "+outf_name+" ", output="<td>";
#ifdef SHOW_LOGS
	cerr << command << endl;
#endif
	output+=test;
	output+="</td>\n";
	// runtime
	timeval ts, te;
	gettimeofday(&ts, NULL);
	int ret=system(command.c_str());
	gettimeofday(&te, NULL);
	double cl=(te.tv_sec+static_cast<double>(te.tv_usec)/1000000)-(ts.tv_sec+static_cast<double>(ts.tv_usec)/1000000);
	// end of runtime && time calculating
	cl*=100;
	cl=floor(cl)/100;
	double dtime_limit=strtod(time_limit.c_str(), NULL);
	if(cl>=dtime_limit) // Time limit
		output+="<td class=\"tl_re\">Time limit</td>\n<td>";
	else if(ret!=0) // Runtime error
	{
		output+="<td class=\"tl_re\">Runtime error</td>\n<td>";
		min_group_ratio=0;
	}
	else // checking answer
	{
		int judge_status=system((checker+_name.substr(2)+"tests/"+test+".in "+_name.substr(2)+"tests/"+test+".out /judge/"+outf_name).c_str())/256;
	#ifdef SHOW_LOGS
		cerr << '\t' << checker+_name.substr(2)+"tests/"+test+".in "+_name.substr(2)+"tests/"+test+".out /judge/"+outf_name << endl;
		cerr << "Checker return: " << judge_status << endl;
	#endif
		if(judge_status==0)
			output+="<td class=\"ok\">OK</td>\n<td>";
		else if(judge_status==1)
		{
			output+="<td class=\"wa\">Wrong answer</td>\n<td>";
			min_group_ratio=0;
		}
		else
		{
			output+="<td style=\"background: #ff7b7b\">Evaluation failure</td>\n<td>";
			min_group_ratio=0;
		}
	} // end of checking answer
	output+=f_time(static_cast<int>(cl*100));
	output+="/";
	output+=f_time(static_cast<int>(dtime_limit*100));
	output+="</td>\n";
	// calculating current_ratio
	cl=dtime_limit-cl;
	if(cl<0)
		cl=0;
	dtime_limit/=2;
	double current_ratio=cl/dtime_limit;
	if(current_ratio<min_group_ratio)
		min_group_ratio=current_ratio;
	remove(outf_name.c_str());
return output;
}

string task::judge(const string& exec_name)
{
	exec=exec_name;
	fstream config((_name+"conf.cfg").c_str(), ios::in);
#ifdef SHOW_LOGS
	cerr << "Openig file: " << _name << "conf.cfg" << endl;
#endif
	if(!config.good())
		return "<pre>Judge Error</pre>";
#ifdef SHOW_LOGS
	cerr << "Success!" << endl;
#endif
	string trashes, checker_name;
	getline(config, trashes); // Task tag
	getline(config, trashes); // Task name
	// Checker
	config >> checker_name;
	string checker_exec=string(tmp_dir)+"checker";
#ifdef SHOW_LOGS
	cerr << checker_exec << endl;
#endif
	if(system(("timeout 20 g++ checkers/"+checker_name+".cpp -s -O2 -static -lm -m32 -o "+checker_exec+" > /dev/null 2> /dev/null").c_str())!=0)
		return "<pre>Judge Error (checker compilation)</pre>";
#ifdef SHOW_LOGS
	cerr << "Compilation command: "  << "timeout 20 g++ checkers/"+checker_name+".cpp -s -O2 -static -lm -m32 -o "+checker_exec+" > /dev/null 2> /dev/null" << endl;
#endif
	checker="timeout 20 chroot --userspec=1001 ../ /judge/"+checker_exec+" ";
	// Rest
	config >> memory_limit;
	string out="<table style=\"margin-top: 5px\" class=\"table results\">\n<thead>\n<tr>\n<th style=\"min-width: 70px\">Test</th>\n<th style=\"min-width: 180px\">Result</th>\n<th style=\"min-width: 90px\">Time</th>\n<th style=\"min-width: 60px\">Result</th>\n</tr>\n</thead>\n<tbody>\n";
	long long max_score=0, total_score=0, group_score;
	string test_name, time_limit, group_buffer;
	int other_tests=0;
	while(config.good())
	{
		config >> test_name >> time_limit;
		if(!other_tests)
		{
			min_group_ratio=1;
			config >> other_tests >> group_score;
			max_score+=group_score;
			out+="<tr>\n";
			out+=check_on_test(test_name, time_limit);
			out+="<td class=\"groupscore\""+string(other_tests>1 ? " rowspan=\""+myto_string(other_tests)+"\"":"")+">";
			group_buffer="";
		}
		else
		{
			group_buffer+="<tr>\n";
			group_buffer+=check_on_test(test_name, time_limit);
			group_buffer+="</tr>\n";
		}
		--other_tests;
		if(!other_tests)
		{
			total_score+=group_score*min_group_ratio;
			out+=myto_string(group_score*min_group_ratio)+"/"+myto_string(group_score)+"</td>\n</tr>\n";
			out+=group_buffer;
		}
	}
	out+="</tbody>\n</table>";
	return "<pre>Score: "+myto_string(total_score)+"/"+myto_string(max_score)+"\nStatus: Judged</pre>\n"+out;
}
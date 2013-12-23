#include "main.hpp"
#include "judge.hpp"
#include <unistd.h> // getpid()
#include <cstdlib>
#include <iostream>
#include <fstream>

using namespace std;

void GetTemplateOfReport(string& template_front, string& template_back, const string& file)
{
	template_front=template_back="";
	string file_content=file_get_contents(file);
	deque<int> div_begins=kmp(file_content, "<div"), div_ends=kmp(file_content, "</div>");
	unsigned start=0, deep=1, finish=file_content.size(); // Deep od <div>
	for(int i=0; i<div_begins.size(); ++i)
	{
		cerr << div_begins[i] << " " << string(file_content.begin()+div_begins[i]+1, file_content.begin()+div_begins[i]+24) << endl;
		if(file_content.compare(div_begins[i]+1, 23, " class=\"submit_status\">")==0)
		{
			start=div_begins[i]+24;
			cerr << "get: " << start << endl;
			div_begins.erase(div_begins.begin(), div_begins.begin()+i+1);
			break;
		}
	}
	for(int i=0; i<div_begins.size(); ++i)
		cerr << ' ' << div_begins[i];
	cerr << endl;
	for(int i=0; i<div_ends.size(); ++i)
		cerr << ' ' << div_ends[i];
	cerr << endl;
	while(!div_ends.empty() && div_ends.front()<start)
		div_ends.pop_front();
	cerr << "[[[ " << div_ends.size() << " ]]]" << endl;
	while(!div_ends.empty() && deep>0)
	{
		cerr << div_begins.front() << ' ' << div_ends.front() << ' ' << deep << ' ';
		if(!div_begins.empty() && div_begins.front()<div_ends.front())
		{
			cerr << "first: ";
			++deep;
			div_begins.pop_front();
		}
		else
		{
			cerr << "second: ";
			--deep;
			finish=div_ends.front()-6;
			div_ends.pop_front();
		}
		cerr << deep << endl;
	}
	cerr << finish << " ;; " << file_content.size() << endl;
	file_content[finish]='\n';
	template_front.assign(file_content.begin(), file_content.begin()+start);
	template_front+='\n';
	template_back.assign(file_content.begin()+finish, file_content.end());
}

int main(int argc, char** argv)
{
	// check if this process isn't oldest
	// if(system(("if test `pgrep -x --oldest judge` = "+myto_string(getpid())+" ; then exit 0; else exit 1; fi").c_str())) return 1;
	/*if(argc<3)
	{
		cerr << "Usage: spr <task name> <exec name> [test names...]\n";
		return 1;
	}
	task judge(argv[1]);
	judge.judge(argc-2, argv+2);*/
	while(!reports_queue::empty())
	{
		cerr << reports_queue::front() << endl;
		string report_id, task_id;
		fstream queue_file(("queue/"+reports_queue::front()).c_str(), ios::in);
		if(queue_file.good())
		{
			getline(queue_file, report_id);
			getline(queue_file, task_id);
			queue_file.close();
		}
		string report_front, report_back, report_name="../reports/"+report_id+".php";
		GetTemplateOfReport(report_front, report_back, report_name);
		fstream report;
		if(!compile::run(report_id, "exec.e"))
		{
			if(report.open(report_name.c_str(), ios::out), report.good())
			{
				report << report_front << "<pre>Status: Compilation failed</pre>\n<pre>Points: 0<pre>\n<pre>" << make_safe_php_string(make_safe_html_string(compile::compile_errors)) << "</pre>" << report_back;
				report.close();
			}
		}
		else
		{
			if(report.open(report_name.c_str(), ios::out), report.good())
			{
				report << report_front << "<pre>Status: Judging...</pre>" << report_back;
				report.close();
			}
			task rated_task("../tasks/"+task_id);
			string tmp=rated_task.judge();
			if(report.open(report_name.c_str(), ios::out), report.good())
			{
				report << report_front << tmp << report_back;
				report.close();
			}
			remove("chroot/exec.e");
		}
		reports_queue::pop();
	}
return 0;
}

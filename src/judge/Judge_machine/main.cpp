#include "main.hpp"
#include "judge.hpp"
#include <unistd.h> // getpid()
#include <iostream>
#include <fstream>
#include <csignal>
#include <dirent.h>

using namespace std;

void GetTemplateOfReport(string& template_front, string& template_back, const string& file)
{
	template_front=template_back="";
	string file_content=file_get_contents(file);
	deque<unsigned> div_begins=kmp(file_content, "<div"), div_ends=kmp(file_content, "</div>");
	unsigned start=0, deep=1, finish=file_content.size(); // Deep od <div>
	for(unsigned i=0; i<div_begins.size(); ++i)
	{
		D(cerr << div_begins[i] << " " << string(file_content.begin()+div_begins[i]+1, file_content.begin()+div_begins[i]+24) << endl);
		if(file_content.compare(div_begins[i]+1, 23, " class=\"submit_status\">")==0)
		{
			start=div_begins[i]+24;
			D(cerr << "get: " << start << endl);
			div_begins.erase(div_begins.begin(), div_begins.begin()+i+1);
			break;
		}
	}
#ifdef DEBUG
	for(size_t i=0; i<div_begins.size(); ++i)
		cerr << ' ' << div_begins[i];
	cerr << endl;
	for(size_t i=0; i<div_ends.size(); ++i)
		cerr << ' ' << div_ends[i];
	cerr << endl;
#endif
	while(!div_ends.empty() && div_ends.front()<start)
		div_ends.pop_front();
	D(cerr << "[[[ " << div_ends.size() << " ]]]" << endl);
	while(!div_ends.empty() && deep>0)
	{
		D(cerr << div_begins.front() << ' ' << div_ends.front() << ' ' << deep << ' ');
		if(!div_begins.empty() && div_begins.front()<div_ends.front())
		{
			D(cerr << "first: ");
			++deep;
			div_begins.pop_front();
		}
		else
		{
			D(cerr << "second: ");
			--deep;
			finish=div_ends.front()-6;
			div_ends.pop_front();
		}
		D(cerr << deep << endl);
	}
	D(cerr << finish << " ;; " << file_content.size() << endl);
	file_content[finish]='\n';
	template_front.assign(file_content.begin(), file_content.begin()+start);
	template_front+='\n';
	template_back.assign(file_content.begin()+finish, file_content.end());
}

temporary_directory tmp_dir("judge_machine.XXXXXX\0");
string *public_report_name, *public_report_front, *public_report_back;

void control_exit(int=0)
{
	// Repair status of current report
	fstream report;
	if(report.open(public_report_name->c_str(), ios::out), report.good())
	{
		report << *public_report_front << "<pre>Status: Waiting for judge...</pre>" << *public_report_back;
		report.close();
	}
	D(cerr << "Removing temporary directory" << endl);
	remove_r(tmp_dir);
	exit(1);
}

int main()
{
	// signal control
	signal(SIGHUP, control_exit);
	signal(SIGINT, control_exit);
	signal(SIGQUIT, control_exit);
	signal(SIGILL, control_exit);
	signal(SIGTRAP, control_exit);
	signal(SIGABRT, control_exit);
	signal(SIGIOT, control_exit);
	signal(SIGBUS, control_exit);
	signal(SIGFPE, control_exit);
	signal(SIGKILL, control_exit); // We won't block SIGKILL
	signal(SIGUSR1, control_exit);
	signal(SIGSEGV, control_exit);
	signal(SIGUSR2, control_exit);
	signal(SIGPIPE, control_exit);
	signal(SIGALRM, control_exit);
	signal(SIGTERM, control_exit);
	signal(SIGSTKFLT, control_exit);
	signal(_NSIG, control_exit);
	// check if this process isn't oldest
	if(system(("if test `pgrep -x -o judge_machine` = "+myto_string(getpid())+" ; then exit 0; else exit 1; fi").c_str())) return 1;
	// checking reports
	while(!reports_queue::empty())
	{
		D(cerr << reports_queue::front() << endl);
		string report_id, task_id;
		fstream queue_file(("queue/"+reports_queue::front()).c_str(), ios::in);
		if(queue_file.good())
		{
			getline(queue_file, report_id);
			getline(queue_file, task_id);
			queue_file.close();
		}
		string report_front, report_back, report_name="../public/reports/"+report_id+".php";
		GetTemplateOfReport(report_front, report_back, report_name);
		public_report_name=&report_name;
		public_report_front=&report_front;
		public_report_back=&report_back;
		fstream report;
		char exec[]="chroot/exec.XXXXXX";
		if(-1==mkstemp(exec))
		{
			cerr << "Cannot create exec file in chroot/";
			control_exit();
		}
		else
			remove(exec);
		if(!compile::run(report_id, exec))
		{
			D(cerr << "Compilation failed" << endl);
			if(report.open(report_name.c_str(), ios::out), report.good())
			{
				report << report_front << "<pre>Status: Compilation failed</pre>\n<pre>Points: 0<pre>\n<pre>" << make_safe_php_string(make_safe_html_string(compile::run.GetCompileErrors())) << "</pre>" << report_back;
				report.close();
			}
		}
		else
		{
			D(cerr << "Compilation success" << endl);
			if(report.open(report_name.c_str(), ios::out), report.good())
			{
				report << report_front << "<pre>Status: Judging...</pre>" << report_back;
				report.close();
			}
			task rated_task("../tasks/"+task_id);
			string tmp=rated_task.judge(string(exec+7, exec+18));
			if(report.open(report_name.c_str(), ios::out), report.good())
			{
				report << report_front << tmp << report_back;
				report.close();
			}
			remove(exec);
		}
		reports_queue::pop();
	}
return 0;
}

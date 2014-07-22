#include "main.hpp"
#include "judge.hpp"
#include <unistd.h> // getpid()
#include <iostream>
#include <fstream>
#include <csignal>
#include <dirent.h>

using namespace std;

void GetTemplateOfSubmission(string& template_front, string& template_back, const string& file)
{
	template_front=template_back="";
	string file_content=file_get_contents(file);
	deque<unsigned> div_begins=kmp(file_content, "<div"), div_ends=kmp(file_content, "</div>");
	unsigned start=0, deep=1, finish=file_content.size(); // Deep od <div>
	for(unsigned i=0; i<div_begins.size(); ++i)
	{
		D(cerr << div_begins[i] << " " << string(file_content.begin()+div_begins[i]+1, file_content.begin()+div_begins[i]+24) << endl);
		if(file_content.compare(div_begins[i]+1, 23, " class=\"submit-status\">")==0)
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
string *public_submission_name, *public_submission_front, *public_submission_back;

void control_exit(int=0)
{
	// Repair status of current submission
	fstream submission;
	if(submission.open(public_submission_name->c_str(), ios::out), submission.good())
	{
		submission << *public_submission_front << "<pre>Status: Waiting for judge...</pre>" << *public_submission_back;
		submission.close();
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
	// signal(SIGKILL, control_exit); // We won't block SIGKILL
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
	// checking submissions
	while(!submissions_queue::empty())
	{
		D(cerr << "JUDGING:\n" << submissions_queue::front() << endl;)
		submissions_queue::submission const & rep = submissions_queue::front();
		// fstream queue_file(("queue/"+submissions_queue::front().id()).c_str(), ios::in);
		// if(queue_file.good())
		// {
		// 	getline(queue_file, submission_id);
		// 	getline(queue_file, task_id);
		// 	queue_file.close();
		// }
		string submission_front, submission_back, submission_name="../public/submissions/"+rep.id()+".php";
		GetTemplateOfSubmission(submission_front, submission_back, submission_name);
		public_submission_name=&submission_name;
		public_submission_front=&submission_front;
		public_submission_back=&submission_back;
		fstream submission;
		if(submission.open(submission_name.c_str(), ios::out), submission.good())
		{
			submission << submission_front << "<pre>Status: Judging...</pre>" << submission_back;
			submission.close();
		}
		char exec[]="chroot/exec.XXXXXX";
		if(-1==mkstemp(exec))
		{
			cerr << "Cannot create exec file in chroot/" << endl;
			control_exit();
		}
		else
			remove(exec);
		if(!compile::run(rep.id(), exec))
		{
			D(cerr << "Compilation failed" << endl);
			if(submission.open(submission_name.c_str(), ios::out), submission.good())
			{
				submission << submission_front << "<pre>Status: Compilation failed</pre>\n<pre>Points: 0<pre>\n<pre>" << make_safe_php_string(make_safe_html_string(compile::run.GetCompileErrors())) << "</pre>" << submission_back;
				submissions_queue::front().set(submissions_queue::C_ERROR, 0);
				submission.close();
			}
		}
		else
		{
			D(cerr << "Compilation success" << endl);
			task rated_task("../tasks/"+rep.task_id());
			string tmp=rated_task.judge(string(exec+7, exec+18));
			if(submission.open(submission_name.c_str(), ios::out), submission.good())
			{
				submission << submission_front << tmp << submission_back;
				submission.close();
			}
			remove(exec);
		}
		submissions_queue::pop();
	}
return 0;
}

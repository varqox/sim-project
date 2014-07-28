#include "main.hpp"
#include "judge.hpp"
#include <unistd.h> // getpid()
#include <iostream>
#include <fstream>
#include <csignal>
#include <dirent.h>

using namespace std;

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
	// if(system(("if test `pgrep -x -o judge_machine` = "+myto_string(getpid())+" ; then exit 0; else exit 1; fi").c_str())) return 1;
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
		string submission_name="../public/submissions/"+rep.id()+".php";
		fstream submission;
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
			submissions_queue::front().set(submissions_queue::C_ERROR, 0);
		}
		else
		{
			D(cerr << "Compilation success" << endl);
			task rated_task("../tasks/"+rep.task_id());
			string tmp=rated_task.judge(string(exec+7, exec+18));
			if(submission.open(submission_name.c_str(), ios::out), submission.good())
			{
				submission << "<?php\nrequire_once $_SERVER['DOCUMENT_ROOT'].\"/../php/submission.php\";\ntemplate(" << rep.id() << ",NULL,'" << tmp << "');\n?>";
				submission.close();
			}
			remove(exec);
		}
		submissions_queue::pop();
	}
return 0;
}

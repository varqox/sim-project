#include "main.h"
#include "judge.hpp"
#include <unistd.h> // getpid()
#include <iostream>
#include <fstream>
#include <csignal>
#include <dirent.h>

using namespace std;

temporary_directory tmp_dir("judge_machine.XXXXXX");
string *public_submission_name, *public_submission_front, *public_submission_back;

int main()
{
	// Signal control
	struct sigaction sa;
	memset (&sa, 0, sizeof(sa));
	sa.sa_handler = &exit;

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGABRT, &sa, NULL);
	sigaction(SIGFPE, &sa, NULL);

	// Check if this process isn't oldest
	if(system(("if test `pgrep -x -o judge_machine` = "+myto_string(getpid())+" ; then exit 0; else exit 1; fi").c_str()))
		return 1;

	// Checking submissions
	while(!submissions_queue::empty())
	{
		D(cerr << "JUDGING:\n" << submissions_queue::front() << endl;)
		submissions_queue::submission const & rep = submissions_queue::front();

		string submission_name="../public/submissions/"+rep.id()+".php";
		fstream submission;
		char exec[]="chroot/exec.XXXXXX";
		if(-1==mkstemp(exec))
		{
			cerr << "Cannot create exec file in chroot/" << endl;
			exit(2);
		}
		else
			remove(exec);
		if(0 != compile("../solutions/" << rep.id() << ".cpp", exec))
		{
			D(cerr << "Compilation failed" << endl);
			submissions_queue::front().set(submissions_queue::C_ERROR, 0);
		}
		else
		{
			D(cerr << "Compilation success" << endl);
			Problem rated_problem("../problems/"+rep.problem_id());
			Problem::JudgeResult res = rated_problem.judge(string(exec+7, exec+18));
			if(submission.open(submission_name.c_str(), ios::out), submission.good())
			{
				submission << "<?php\nrequire_once $_SERVER['DOCUMENT_ROOT'].\"/../php/submission.php\";\ntemplate(" << rep.id() << ",'" << make_safe_php_string(res.initial.tests) << "','" << make_safe_php_string(res.initial.comments) << "','" << make_safe_php_string(res.final.tests) << "','" << make_safe_php_string(res.final.comments) << "');\n?>";
				submission.close();
			}
			remove(exec);
		}
		submissions_queue::pop();
	}
return 0;
}

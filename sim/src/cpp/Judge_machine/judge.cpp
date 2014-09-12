#include "judge.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <cstdio>
#include <fstream>
#include <deque>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <csignal>
#include <unistd.h>
#include "main.h"

using namespace std;

namespace runtime
{
	enum res_stat_set {RES_OK, RES_WA, RES_TL, RES_RTE, RES_EVF};
	res_stat_set res_stat = RES_OK;
	int r_val;
	long long cl, time_limit, memory_limit;
	//                        ^ in bytes
	pid_t cpid;
	char *input, *output, *answer, *exec, *checker;

	void on_timelimit(int)
	{
		if(kill(cpid, SIGKILL) == 0)
			res_stat = RES_TL,
			r_val = 137;
	}

	void on_checker_timelimit(int)
	{
		if(kill(cpid, SIGKILL) == 0)
			res_stat = RES_EVF;
	}

	int run(void*)
	{
		struct sigaction sa;
		struct itimerval timer, zero;

		// Set handler
		memset (&sa, 0, sizeof(sa));
		sa.sa_handler = &on_timelimit;
		sigaction(SIGALRM, &sa, NULL);

		cl = time_limit;
		// Set timer
		timer.it_value.tv_sec = time_limit / 1000000;
		timer.it_value.tv_usec = time_limit - timer.it_value.tv_sec * 1000000;
		timer.it_interval.tv_sec = timer.it_interval.tv_usec = 0;
		memset(&zero, 0, sizeof(zero));

		// Run timer (time limit)
		setitimer(ITIMER_REAL, &timer, NULL);

		if((cpid = fork()) == 0)
		{
			// Set up enviroment
			freopen(input, "r", stdin);
			freopen(answer, "w", stdout);
			freopen("/dev/null", "w", stderr);
			if(chroot("chroot") != 0 || chdir("/") != 0 || (setgid((gid_t)1001) || setuid((uid_t)1001)))
				_exit(-1);

			// Set virtual memory limit
			struct rlimit limit;
			limit.rlim_max = limit.rlim_cur = memory_limit;
			prlimit(getpid(), RLIMIT_AS, &limit, NULL);

			// Set processes creating limit
			limit.rlim_max = limit.rlim_cur = 0;
			prlimit(getpid(), RLIMIT_NPROC, &limit, NULL);

			char *arg[] = {NULL};
			execve(exec, arg, arg);
			_exit(-1);
		}
		waitpid(cpid, &r_val, 0);
		setitimer(ITIMER_REAL, &zero, &timer);
		cl -= timer.it_value.tv_sec * 1000000LL + timer.it_value.tv_usec;
		if(WIFEXITED(r_val))
			r_val = WEXITSTATUS(r_val);
		else if(WIFSIGNALED(r_val))
			r_val = WTERMSIG(r_val) + 128;
		else
			// Shouldn't happen. Unknown status...
			r_val = EXIT_FAILURE;
		D(eprint("\tReturned %i\ttime: %.6lf s\n", r_val, cl/1000000.0);)
		if(r_val == 0)
		{
			// Set right handler
			sa.sa_handler = &on_checker_timelimit;
			sigaction(SIGALRM, &sa, NULL);

			// Set timer
			timer.it_value.tv_sec = 20;
			timer.it_value.tv_usec = timer.it_interval.tv_sec = timer.it_interval.tv_usec = 0;

			// Run timer (time limit)
			setitimer(ITIMER_REAL, &timer, NULL);

			if((cpid = fork()) == 0)
			{
				// Set up enviroment
				freopen("/dev/null", "r", stdin);
				freopen("/dev/null", "w", stdout);
				freopen("/dev/null", "w", stderr);
				if(chroot("..") != 0 || chdir("/judge") != 0 || (setgid((gid_t)1001) || setuid((uid_t)1001)))
					_exit(-1);

				// Set virtual memory limit
				struct rlimit limit;
				limit.rlim_max = limit.rlim_cur = 128 * 1024 * 1024;
				prlimit(getpid(), RLIMIT_AS, &limit, NULL);

				// Set processes creating limit
				limit.rlim_max = limit.rlim_cur = 0;
				prlimit(getpid(), RLIMIT_NPROC, &limit, NULL);

				char *arg[] = {checker, input, output, answer, NULL}, *env[] = {NULL};
				execve(checker, arg, env);
				_exit(-1);
			}
			int status;
			waitpid(cpid, &status, 0);
			setitimer(ITIMER_REAL, &zero, &timer);
			if(WIFEXITED(status))
			{
				if(WEXITSTATUS(status) == 0)
					res_stat = RES_OK;
				else if(WEXITSTATUS(status) == 1)
					res_stat = RES_WA;
				else
					res_stat = RES_EVF;
				D(status = WEXITSTATUS(status);)
			}
			else
				D(status = WTERMSIG(status) + 128,)
				res_stat = RES_EVF;
			D(eprint("\tChecker returned: %i\ttime: %.6lf s\n", status, 20.0 - timer.it_value.tv_sec - timer.it_value.tv_usec/1000000.0);)

		}
		else if(cl == time_limit)
			res_stat = RES_TL;
		else
			res_stat = RES_RTE;
		return 0;
	}
}

string task::check_on_test(const string& test, const string& time_limit)
{
	// Setup runtime variables
	string tmp = _name+"tests/"+test+".in";
	runtime::input = new char[tmp.size()+1];
	strcpy(runtime::input, tmp.c_str());

	tmp = _name+"tests/"+test+".out";
	runtime::output = new char[tmp.size()+1];
	strcpy(runtime::output, tmp.c_str());

	runtime::answer = const_cast<char*>(outf_name.c_str());

	tmp = "/"+exec;
	runtime::exec = new char[tmp.size()+1];
	strcpy(runtime::exec, tmp.c_str());

	runtime::checker = const_cast<char*>(checker.c_str());

	// Convert memory limit format from string (KB) to long long (B)
	runtime::memory_limit = 0;
	for(int len = memory_limit.size(), i = 0; i < len; ++i)
		runtime::memory_limit *= 10,
		runtime::memory_limit += memory_limit[i] - '0';
	runtime::memory_limit *= 1024;

	// Time limit
	runtime::time_limit = strtod(time_limit.c_str(), NULL) * 1000000;

	// Runtime
	D(eprint("\nRunning on: %s\tlimits -> (TIME: %s s, MEM: %s KB)\n", test.c_str(), time_limit.c_str(), memory_limit.c_str());)
	pid_t pid = clone(runtime::run, (char*)malloc(4 * 1024 * 1024) + 4 * 1024 * 1024, CLONE_VM | SIGCHLD, NULL);
	waitpid(pid, NULL, 0);

	// Free resources
	delete[] runtime::input;
	delete[] runtime::output;
	delete[] runtime::exec;

	string output = "<td>" + test + "</td>";

	switch(runtime::res_stat)
	{
		case runtime::RES_OK:
			output += "<td class=\"ok\">OK</td>";
			D(eprint("\tStatus: OK\n");)
			break;
		case runtime::RES_WA:
			output += "<td class=\"wa\">Wrong answer</td>";
			D(eprint("\tStatus: Wrong answer\n");)
			min_group_ratio=0;
			break;
		case runtime::RES_TL:
			output += "<td class=\"tl-re\">Time limit</td>";
			D(eprint("\tStatus: Time limit\n");)
			min_group_ratio=0;
			break;
		case runtime::RES_RTE:
			output += "<td class=\"tl-re\">Runtime error</td>";
			D(eprint("\tStatus: Runtime error\n");)
			min_group_ratio=0;
			break;
		case runtime::RES_EVF:
			output += "<td style=\"background:#ff7b7b\">Evaluation failure</td>";
			D(eprint("\tStatus: Evaluation failure\n");)
			min_group_ratio=0;
	}

	output += "<td>";
	tmp = myto_string(runtime::cl/10000);
	tmp.insert(0, 3-tmp.size(), '0');
	tmp.insert(tmp.size()-2, 1, '.');
	output += tmp + "/";
	tmp = myto_string(runtime::time_limit/10000);
	tmp.insert(0, 3-tmp.size(), '0');
	tmp.insert(tmp.size()-2, 1, '.');
	output += tmp + "</td>";

	// Calculate ratio for current test
	double current_ratio = 2.0 - 2.0 * floor(runtime::cl/10000) / floor(runtime::time_limit/10000);
	if(current_ratio < min_group_ratio)
		min_group_ratio = current_ratio;
	remove(outf_name.c_str());
	return output;
}

pair<string, string> task::judge(const string& exec_name)
{
	exec=exec_name;
	fstream config((_name+"conf.cfg").c_str(), ios::in);
	D(cerr << "Openig file: " << _name << "conf.cfg" << endl);
	if(!config.good())
	{
		// Judge Error
		submissions_queue::front().set(submissions_queue::ERROR, 0);
		return make_pair("", "");
	}
	D(cerr << "Success!" << endl);
	string trashes, checker_name;
	getline(config, trashes); // Task tag
	getline(config, trashes); // Task name
	// Checker
	config >> checker_name;
	string checker_exec=string(tmp_dir)+"checker";
	D(cerr << checker_exec << endl);
	if(0 != compile("checkers/" << checker_name << ".cpp", checker_exec))
	{
		D(cerr << "Judge Error (checker compilation failed)" << endl;)
		submissions_queue::front().set(submissions_queue::ERROR, 0);
		return make_pair("", "");
	}
	D(cerr << "Compilation command: "  << "timeout 20 g++ checkers/"+checker_name+".cpp -s -O2 -static -lm -m32 -o "+checker_exec+" > /dev/null 2> /dev/null" << endl);
	checker="/judge/"+checker_exec;
	// Rest
	config >> memory_limit;
	pair<string, string> out;
	long long max_score=0, total_score=0, group_score;
	string test_name, time_limit, group_buffer;
	int other_tests=0;
	bool status_ok = true;
	D(eprint("=================================================================\n");)
	while(config >> test_name >> time_limit)
	{
		if(!other_tests)
		{
			min_group_ratio=1;
			config >> other_tests >> group_score;
			max_score+=group_score;
			if(group_score == 0) {
				out.first += "<tr>";
				out.first += check_on_test(test_name, time_limit);
			} else {
				out.second += "<tr>";
				out.second += check_on_test(test_name, time_limit);
			}
			if(runtime::res_stat != runtime::RES_OK && group_score == 0)
				status_ok = false;
			if(group_score == 0) {
				out.first += "<td class=\"groupscore\""+string(other_tests>1 ? " rowspan=\""+myto_string(other_tests)+"\"":"")+">";
			} else {
				out.second += "<td class=\"groupscore\""+string(other_tests>1 ? " rowspan=\""+myto_string(other_tests)+"\"":"")+">";
			}
			group_buffer="";
		}
		else
		{
			group_buffer+="<tr>";
			group_buffer+=check_on_test(test_name, time_limit);
			if(runtime::res_stat != runtime::RES_OK && group_score == 0)
				status_ok = false;
			group_buffer+="</tr>\n";
		}
		--other_tests;
		if(!other_tests)
		{
			total_score+=group_score*min_group_ratio;
			if (group_score == 0) {
				out.first += myto_string(group_score*min_group_ratio)+"/"+myto_string(group_score)+"</td></tr>\n";
				out.first += group_buffer;
			} else {
				out.second += myto_string(group_score*min_group_ratio)+"/"+myto_string(group_score)+"</td></tr>\n";
				out.second += group_buffer;
			}
		}
	}
	D(eprint("=================================================================\nScore: %lli / %lli\n", total_score, max_score);)
	submissions_queue::front().set(status_ok ? submissions_queue::OK : submissions_queue::ERROR, total_score);
	return out;
}
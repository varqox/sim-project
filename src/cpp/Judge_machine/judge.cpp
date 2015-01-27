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
#include <cassert>
#include <sys/fcntl.h>
#include <unistd.h>
#include "main.h"

using namespace std;

namespace runtime
{
	enum ResultStatusSet {RES_OK, RES_WA, RES_TL, RES_RTE, RES_EVF};
	ResultStatusSet res_stat = RES_OK;
	int r_val;
	long long cl, time_limit, memory_limit;
	//                        ^ in bytes
	pid_t cpid;
	char *input, *output, *answer, *exec, *checker;
	string comments;

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
		comments.clear();

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
		else if(WIFSIGNALED(r_val)) {
			r_val = WTERMSIG(r_val) + 128;
			comments = "Process exited due to signal " + myto_string(r_val - 128);
		}
		else
			// Shouldn't happen. Unknown status...
			r_val = EXIT_FAILURE;
		D(eprint("\tReturned %i\ttime: %.6lf s\n", r_val, cl/1000000.0);)
		if(r_val == 0)
		{
			int checker_out = open((string(tmp_dir) + "checker_out").c_str(),  O_CREAT|O_RDWR|O_TRUNC, S_IRUSR | S_IWUSR);
			if (checker_out == -1) {
			error_chck_out:
				res_stat = RES_EVF;
				comments = "Cannot open/create file: checker_out";
				return 1;
			}
			unlink((string(tmp_dir) + "checker_out").c_str()); // assert that checker will not be able to access it

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
				// Change stdout
				int stdout_d = fileno(stdout);
				close(stdout_d); // Close stdout
				dup2(checker_out, stdout_d); // Replace it with file

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

			// Take care of checker_out
			if (res_stat != RES_OK) {
				// We will parse only first line
				size_t len = 0;
				char *line = NULL;
				FILE *lll = fdopen(checker_out, "r");
				// It cannot be NULL but for certainty:
				if (lll == NULL)
					goto error_chck_out;
				lseek(checker_out, 0, SEEK_SET); // move read pointer to the beggining
				ssize_t read = getline(&line, &len, lll);

				// Remove trailing white charaters from line
				while (read > 0 && isspace(line[read - 1]))
					line[--read] = '\0';

				if (read > 0) {
					comments.assign(line, line + min(read, ssize_t(200)));
					if (read > 200)
						comments += "...";
				}
				if (line)
					delete[] line;
				fclose(lll); // We need to destroy FILE object (and close checker_out)
			} else
				close(checker_out); // We have to close checker_out
		}
		else if(cl == time_limit) {
			res_stat = RES_TL;
			comments = "Time limit exceeded";
		}
		else {
			res_stat = RES_RTE;
			if (comments.empty()) // No signal detected
				comments = "Runtime error: exit code " + myto_string(res_stat);
		}
		return 0;
	}
}

Problem::JudgeResult::Group Problem::check_on_test(const string& test, const string& time_limit)
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

	JudgeResult::Group output;
	output.tests = "<td>" + test + "</td>";

	if (runtime::comments.size()) {
		output.comments += "<li><span class=\"test-id\">" + test + "</span>" + runtime::comments + "</li>\n";
	}

	switch(runtime::res_stat)
	{
		case runtime::RES_OK:
			output.tests += "<td class=\"ok\">OK</td>";
			D(eprint("\tStatus: OK\n");)
			break;
		case runtime::RES_WA:
			output.tests += "<td class=\"wa\">Wrong answer</td>";
			D(eprint("\tStatus: Wrong answer\n");)
			min_group_ratio=0;
			break;
		case runtime::RES_TL:
			output.tests += "<td class=\"tl-re\">Time limit</td>";
			D(eprint("\tStatus: Time limit\n");)
			min_group_ratio=0;
			break;
		case runtime::RES_RTE:
			output.tests += "<td class=\"tl-re\">Runtime error</td>";
			D(eprint("\tStatus: Runtime error\n");)
			min_group_ratio=0;
			break;
		case runtime::RES_EVF:
			output.tests += "<td style=\"background:#ff7b7b\">Evaluation failure</td>";
			D(eprint("\tStatus: Evaluation failure\n");)
			min_group_ratio=0;
	}

	output.tests += "<td>";
	tmp = myto_string(runtime::cl/10000);
	tmp.insert(0, 3-tmp.size(), '0');
	tmp.insert(tmp.size()-2, 1, '.');
	output.tests += tmp + "/";
	tmp = myto_string(runtime::time_limit/10000);
	tmp.insert(0, 3-tmp.size(), '0');
	tmp.insert(tmp.size()-2, 1, '.');
	output.tests += tmp + "</td>";

	// Calculate ratio for current test
	double current_ratio = 2.0 - 2.0 * floor(runtime::cl/10000) / floor(runtime::time_limit/10000);
	if(current_ratio < min_group_ratio)
		min_group_ratio = current_ratio;
	remove(outf_name.c_str());
	return output;
}

Problem::JudgeResult Problem::judge(const string& exec_name)
{
	exec=exec_name;
	fstream config((_name+"conf.cfg").c_str(), ios::in);
	D(cerr << "Openig file: " << _name << "conf.cfg" << endl);
	if(!config.good())
	{
		// Judge Error
		submissions_queue::front().set(submissions_queue::ERROR, 0);
		return JudgeResult();
	}
	D(cerr << "Success!" << endl);
	string trashes, checker_name;
	getline(config, trashes); // Problem tag
	getline(config, trashes); // Problem name
	// Checker
	config >> checker_name;
	string checker_exec=string(tmp_dir)+"checker";
	D(cerr << checker_exec << endl);
	if(0 != compile("checkers/" << checker_name << ".cpp", checker_exec))
	{
		D(cerr << "Judge Error (checker compilation failed)" << endl;)
		submissions_queue::front().set(submissions_queue::ERROR, 0);
		return JudgeResult();
	}
	D(cerr << "Compilation command: "  << "timeout 20 g++ checkers/"+checker_name+".cpp -s -O2 -static -lm -m32 -o "+checker_exec+" > /dev/null 2> /dev/null" << endl);
	checker="/judge/"+checker_exec;
	// Rest
	config >> memory_limit;
	JudgeResult result;
	long long max_score=0, total_score=0, group_score;
	string test_name, time_limit, group_buffer;
	int other_tests=0;
	bool status_ok = true;
	D(eprint("=================================================================\n");)
	JudgeResult::Group tmp;
	while(config >> test_name >> time_limit)
	{
		if(!other_tests)
		{
			min_group_ratio=1;
			config >> other_tests >> group_score;
			max_score+=group_score;
			if(group_score == 0) {
				result.initial.tests += "<tr>";
				tmp = check_on_test(test_name, time_limit);
				result.initial.tests += tmp.tests;
				result.initial.comments += tmp.comments;
			} else {
				result.final.tests += "<tr>";
				tmp = check_on_test(test_name, time_limit);
				result.final.tests += tmp.tests;
				result.final.comments += tmp.comments;
			}
			if(runtime::res_stat != runtime::RES_OK && group_score == 0)
				status_ok = false;
			if(group_score == 0)
				result.initial.tests += "<td class=\"groupscore\""+string(other_tests>1 ? " rowspan=\""+myto_string(other_tests)+"\"":"")+">";
			else
				result.final.tests += "<td class=\"groupscore\""+string(other_tests>1 ? " rowspan=\""+myto_string(other_tests)+"\"":"")+">";
			group_buffer = "";
		}
		else
		{
			group_buffer += "<tr>";
			tmp = check_on_test(test_name, time_limit);
			group_buffer += tmp.tests;
			if (group_score == 0)
				result.initial.comments += tmp.comments;
			else
				result.final.comments += tmp.comments;
			if(runtime::res_stat != runtime::RES_OK && group_score == 0)
				status_ok = false;
			group_buffer += "</tr>\n";
		}
		--other_tests;
		if(!other_tests)
		{
			total_score += group_score*min_group_ratio;
			if (group_score == 0) {
				result.initial.tests += myto_string(group_score*min_group_ratio)+"/"+myto_string(group_score)+"</td></tr>\n";
				result.initial.tests += group_buffer;
			} else {
				result.final.tests += myto_string(group_score*min_group_ratio)+"/"+myto_string(group_score)+"</td></tr>\n";
				result.final.tests += group_buffer;
			}
		}
	}
	D(eprint("=================================================================\nScore: %lli / %lli\n", total_score, max_score);)
	submissions_queue::front().set(status_ok ? submissions_queue::OK : submissions_queue::ERROR, total_score);
	return result;
}

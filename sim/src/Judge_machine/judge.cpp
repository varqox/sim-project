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
#include <fcntl.h>
#include <unistd.h>
#include <linux/limits.h> // PATH_MAX
#include "main.h"

#define eprintf(...) fprintf(stderr, __VA_ARGS__)

using namespace std;

const size_t COMMENTS_MAX = 204;

struct RuntimeInfo {
	enum ResultStatusSet {RES_OK, RES_WA, RES_TL, RES_RTE, RES_EVF};

	char in_file[PATH_MAX], out_file[PATH_MAX], ans_file[PATH_MAX];
	char exec[PATH_MAX], checker[PATH_MAX];
	long long time_limit, memory_limit;
	//        ^ in usec   ^ in bytes
	ResultStatusSet res_stat;
	char comments[COMMENTS_MAX];

	RuntimeInfo(const string& in, const string& out, const string& ans,
			const string& ec, const string& chck, long long tl, long long ml,
			ResultStatusSet rs = RES_OK)
		: time_limit(tl), memory_limit(ml), res_stat(rs) {
			strcpy(in_file, in.c_str());
			strcpy(out_file, out.c_str());
			strcpy(ans_file, ans.c_str());
			strcpy(exec, ec.c_str());
			strcpy(checker, chck.c_str());
			strcpy(comments, "");
		}
};

namespace runtime
{
	RuntimeInfo *rt_stat;
	int r_val;
	pid_t cpid;

	void on_timelimit(int)
	{
		D(eprintf("KILLED\n");)
		if(kill(cpid, SIGKILL) == 0)
			r_val = 137;
	}

	void on_checker_timelimit(int)
	{
		D(eprintf("Checker: KILLED\n");)
		if(kill(cpid, SIGKILL) == 0)
			rt_stat->res_stat = RuntimeInfo::RES_EVF;
	}

	void run(int shm_key)
	{
		// Attach shared memory segment
		rt_stat = (RuntimeInfo*)shmat(shm_key, NULL, 0);
		if (rt_stat == (void*)-1) {
			eprintf("%s: filed to attach shared memory segment\n", __PRETTY_FUNCTION__);
			return;
		}

		struct sigaction sa;
		struct itimerval timer, zero;

		// Set handler
		memset (&sa, 0, sizeof(sa));
		sa.sa_handler = &on_timelimit;
		sigaction(SIGALRM, &sa, NULL);

		long long cl = rt_stat->time_limit;
		// Set timer
		timer.it_value.tv_sec = rt_stat->time_limit / 1000000;
		timer.it_value.tv_usec = rt_stat->time_limit - timer.it_value.tv_sec * 1000000;
		timer.it_interval.tv_sec = timer.it_interval.tv_usec = 0;
		memset(&zero, 0, sizeof(zero));

		// Run timer (time limit)
		setitimer(ITIMER_REAL, &timer, NULL);

		if((cpid = fork()) == 0)
		{
			// Set up environment
			freopen(rt_stat->in_file, "r", stdin);
			freopen(rt_stat->ans_file, "w", stdout);
			freopen("/dev/null", "w", stderr);
			if(chroot("chroot") != 0 || chdir("/") != 0 || (setgid((gid_t)1001) || setuid((uid_t)1001)))
				_exit(-1);

			// Set virtual memory limit
			struct rlimit limit;
			limit.rlim_max = limit.rlim_cur = rt_stat->memory_limit;
			prlimit(getpid(), RLIMIT_AS, &limit, NULL);

			// Set processes creating limit
			limit.rlim_max = limit.rlim_cur = 0;
			prlimit(getpid(), RLIMIT_NPROC, &limit, NULL);

			char *arg[] = {NULL};
			execve(rt_stat->exec, arg, arg);
			_exit(-1);
		} else if (cpid == -1) {
			eprintf("%s: filed to fork()\n", __PRETTY_FUNCTION__);
			rt_stat->res_stat = RuntimeInfo::RES_RTE;
			rt_stat->time_limit = cl;
			shmdt(rt_stat); // Detach shared memory segment
			return;
		}
		waitpid(cpid, &r_val, 0);

		setitimer(ITIMER_REAL, &zero, &timer);
		cl -= timer.it_value.tv_sec * 1000000LL + timer.it_value.tv_usec;

		if(WIFEXITED(r_val))
			r_val = WEXITSTATUS(r_val);
		else if(WIFSIGNALED(r_val)) {
			r_val = WTERMSIG(r_val) + 128;
			strcpy(rt_stat->comments, ("Process exited due to signal " + toString(r_val - 128)).c_str());
		}
		else
			// Shouldn't happen. Unknown status...
			r_val = EXIT_FAILURE;

		D(eprint("\tReturned %i\ttime: %.6lf s\n", r_val, cl/1000000.0);)

		if(r_val == 0) {
			int checker_out = open((string(tmp_dir) + "checker_out").c_str(),  O_CREAT|O_RDWR|O_TRUNC, S_IRUSR | S_IWUSR);
			if (checker_out == -1) {
			 error_chck_out:
				rt_stat->res_stat = RuntimeInfo::RES_EVF;
				strcpy(rt_stat->comments, "Cannot open/create file: checker_out");

				rt_stat->time_limit = cl;
				shmdt(rt_stat); // Detach shared memory segment
				return;
			}
			unlink((string(tmp_dir) + "checker_out").c_str()); // assert that checker will not be able to access it

			// Set right handler
			sa.sa_handler = &on_checker_timelimit;
			sigaction(SIGALRM, &sa, NULL);

			// Set timer (20s)
			timer.it_value.tv_sec = 20;
			timer.it_value.tv_usec = timer.it_interval.tv_sec = timer.it_interval.tv_usec = 0;

			// Run timer (time limit)
			setitimer(ITIMER_REAL, &timer, NULL);

			if((cpid = fork()) == 0)
			{
				// Set up environment
				freopen("/dev/null", "r", stdin);
				dup2(checker_out, STDOUT_FILENO); // Replace stdout with file
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

				char *arg[] = {rt_stat->checker, rt_stat->in_file, rt_stat->out_file, rt_stat->ans_file, NULL}, *env[] = {NULL};
				execve(rt_stat->checker, arg, env);
				_exit(-1);
			} else if (cpid == -1) {
				eprintf("%s: filed to fork()\n", __PRETTY_FUNCTION__);
				rt_stat->res_stat = RuntimeInfo::RES_RTE;
				rt_stat->time_limit = cl;
				shmdt(rt_stat); // Detach shared memory segment
				return;
			}
			int status;
			waitpid(cpid, &status, 0);
			setitimer(ITIMER_REAL, &zero, &timer);

			if(WIFEXITED(status)) {
				if(WEXITSTATUS(status) == 0)
					rt_stat->res_stat = RuntimeInfo::RES_OK;
				else if(WEXITSTATUS(status) == 1)
					rt_stat->res_stat = RuntimeInfo::RES_WA;
				else
					rt_stat->res_stat = RuntimeInfo::RES_EVF;
				D(status = WEXITSTATUS(status);)
			} else {
				D(status = WTERMSIG(status) + 128;)
				rt_stat->res_stat = RuntimeInfo::RES_EVF;
			}
			D(eprint("\tChecker returned: %i\ttime: %.6lf s\n", status, 20.0 - timer.it_value.tv_sec - timer.it_value.tv_usec/1000000.0);)

			// Take care of checker_out
			if (rt_stat->res_stat != RuntimeInfo::RES_OK) {
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
					string comments(line, line + min(read, ssize_t(COMMENTS_MAX - 4)));
					if (read > ssize_t(COMMENTS_MAX - 4))
						comments += "...";
					strcpy(rt_stat->comments, comments.c_str());
				}
				if (line)
					delete[] line;
				fclose(lll); // We need to destroy FILE object (and close checker_out)
			} else
				close(checker_out); // We have to close checker_out
		} else if(cl == rt_stat->time_limit) {
			rt_stat->res_stat = RuntimeInfo::RES_TL;
			strcpy(rt_stat->comments, "Time limit exceeded");
		} else {
			rt_stat->res_stat = RuntimeInfo::RES_RTE;
			if (strlen(rt_stat->comments) == 0) // No signal detected
				strcpy(rt_stat->comments, ("Runtime error: exit code " + toString(rt_stat->res_stat)).c_str());
		}
		rt_stat->time_limit = cl;
		shmdt(rt_stat); // Detach shared memory segment
	}
}

static RuntimeInfo::ResultStatusSet runtime_res_stat;

Problem::JudgeResult::Group Problem::check_on_test(const string& test, const string& time_limit, long long mem_limit)
{
	// Time limit
	long long time_limit_ll = strtod(time_limit.c_str(), NULL) * 1000000;

	// Create shared memory segment (only once)
	static SharedMemorySegment shm_sgmt(sizeof(RuntimeInfo));
	if (shm_sgmt.addr() == NULL) {
		eprintf("Failed to get shared memory segment\n");
		JudgeResult::Group output;
		output.tests = "<td>" + test + "</td>";
		output.tests += "<td style=\"background:#ff7b7b\">Evaluation failure</td>";
		runtime_res_stat = RuntimeInfo::RES_EVF;
		min_group_ratio=0;

		output.tests += "<td>0.00/";
		string tmp = toString(time_limit_ll/10000);
		if (tmp.size() < 3)
			tmp.insert(0, 3-tmp.size(), '0');
		tmp.insert(tmp.size()-2, 1, '.');
		output.tests += tmp + "</td>";
		return output;
	}

	// Setup runtime variables
	RuntimeInfo *rt_stat = new (shm_sgmt.addr()) RuntimeInfo(_name + "tests/" + test + ".in", _name + "tests/" + test + ".out", outf_name, "/" + exec, checker, time_limit_ll, mem_limit << 10);

	// Runtime
	D(eprint("\nRunning on: %s\tlimits -> (TIME: %s s, MEM: %lli KB)\n", test.c_str(), time_limit.c_str(), mem_limit);)

	pid_t cpid = fork();
	if (cpid == 0) {
		runtime::run(shm_sgmt.key());
		_exit(0);
	} else if (cpid == -1) {
		eprintf("%s: filed to fork()\n", __PRETTY_FUNCTION__);
		abort();
	}
	waitpid(cpid, NULL, 0);

	JudgeResult::Group output;
	output.tests = "<td>" + test + "</td>";

	if (strlen(rt_stat->comments) > 0)
		output.comments += "<li><span class=\"test-id\">" + test + "</span>" + string(rt_stat->comments) + "</li>\n";

	runtime_res_stat = rt_stat->res_stat;
	switch(rt_stat->res_stat)
	{
		case RuntimeInfo::RES_OK:
			output.tests += "<td class=\"ok\">OK</td>";
			D(eprint("\tStatus: OK\n");)
			break;
		case RuntimeInfo::RES_WA:
			output.tests += "<td class=\"wa\">Wrong answer</td>";
			D(eprint("\tStatus: Wrong answer\n");)
			min_group_ratio=0;
			break;
		case RuntimeInfo::RES_TL:
			output.tests += "<td class=\"tl-re\">Time limit</td>";
			D(eprint("\tStatus: Time limit\n");)
			min_group_ratio=0;
			break;
		case RuntimeInfo::RES_RTE:
			output.tests += "<td class=\"tl-re\">Runtime error</td>";
			D(eprint("\tStatus: Runtime error\n");)
			min_group_ratio=0;
			break;
		case RuntimeInfo::RES_EVF:
			output.tests += "<td style=\"background:#ff7b7b\">Evaluation failure</td>";
			D(eprint("\tStatus: Evaluation failure\n");)
			min_group_ratio=0;
	}

	output.tests += "<td>";
	string tmp = toString(rt_stat->time_limit/10000);
	if (tmp.size() < 3)
		tmp.insert(0, 3-tmp.size(), '0');
	tmp.insert(tmp.size()-2, 1, '.');
	output.tests += tmp + "/";
	tmp = toString(time_limit_ll/10000);
	if (tmp.size() < 3)
		tmp.insert(0, 3-tmp.size(), '0');
	tmp.insert(tmp.size()-2, 1, '.');
	output.tests += tmp + "</td>";

	// Calculate ratio for current test
	double current_ratio = 2.0 - 2.0 * (rt_stat->time_limit/10000) / (time_limit_ll/10000);
	if(current_ratio < min_group_ratio)
		min_group_ratio = current_ratio;
	remove(outf_name.c_str());
	return output;
}

Problem::JudgeResult Problem::judge(const string& exec_name)
{
	exec = exec_name;
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
	long long memory_limit;
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
				tmp = check_on_test(test_name, time_limit, memory_limit);
				result.initial.tests += tmp.tests;
				result.initial.comments += tmp.comments;
			} else {
				result.final.tests += "<tr>";
				tmp = check_on_test(test_name, time_limit, memory_limit);
				result.final.tests += tmp.tests;
				result.final.comments += tmp.comments;
			}
			if(runtime_res_stat != RuntimeInfo::RES_OK && group_score == 0)
				status_ok = false;
			if(group_score == 0)
				result.initial.tests += "<td class=\"groupscore\""+string(other_tests>1 ? " rowspan=\""+toString(other_tests)+"\"":"")+">";
			else
				result.final.tests += "<td class=\"groupscore\""+string(other_tests>1 ? " rowspan=\""+toString(other_tests)+"\"":"")+">";
			group_buffer = "";
		}
		else
		{
			group_buffer += "<tr>";
			tmp = check_on_test(test_name, time_limit, memory_limit);
			group_buffer += tmp.tests;
			if (group_score == 0)
				result.initial.comments += tmp.comments;
			else
				result.final.comments += tmp.comments;
			if(runtime_res_stat != RuntimeInfo::RES_OK && group_score == 0)
				status_ok = false;
			group_buffer += "</tr>\n";
		}
		--other_tests;
		if(!other_tests)
		{
			total_score += group_score*min_group_ratio;
			if (group_score == 0) {
				result.initial.tests += toString(group_score*min_group_ratio)+"/"+toString(group_score)+"</td></tr>\n";
				result.initial.tests += group_buffer;
			} else {
				result.final.tests += toString(group_score*min_group_ratio)+"/"+toString(group_score)+"</td></tr>\n";
				result.final.tests += group_buffer;
			}
		}
	}
	D(eprint("=================================================================\nScore: %lli / %lli\n", total_score, max_score);)
	submissions_queue::front().set(status_ok ? submissions_queue::OK : submissions_queue::ERROR, total_score);
	return result;
}

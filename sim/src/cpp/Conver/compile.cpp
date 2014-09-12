#include "main.h"
#include <cstdlib>
#include <cstdio>

#include <unistd.h>
#include <sys/wait.h>

using namespace std;

CompileClass compile;

int CompileClass::operator()(const std::string& source_file, const std::string& exec_file) {
	D(
		fprintf(stderr, "Compilation: %s to: %s\n", source_file.c_str(), exec_file.c_str());
	)
	pid_t cpid;
	if((cpid = fork()) == 0) {
		// Set up enviroment
		freopen("/dev/null", "r", stdin);
		freopen("/dev/null", "w", stdout);
		freopen((string(tmp_dir) << "compile_errors").c_str(), "w", stderr);

		string command = "timeout 20 g++ " << source_file << " -s -O2 -static -lm -m32 -o " << exec_file;

		char *arg[] = {getusershell(), const_cast<char*>("-c"), const_cast<char*>(command.c_str()), NULL};
		execve(arg[0], arg, __environ);
		_exit(-1);
	}
	int ret;
	waitpid(cpid, &ret, 0);
	errors_.clear();
	if(ret != 0) {
		errors_ = file_get_contents(string(tmp_dir) << "compile_errors");
		D(fprintf(stderr, "Errors:\n%s\n", errors_.c_str()));
		if(errors_.empty())
			errors_ = "Compile time limit";
	}
	remove((string(tmp_dir) << "compile_errors").c_str());
	return ret;
}

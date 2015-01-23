#include "main.h"
#include "other_functions.h"
#include "../../Algorithms/text/trie.hpp"
#include <cstdio>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h> // chmod()
// #include <sys/types.h> // mkdir()
#include <unistd.h>
#include <signal.h>
#include <vector>
#include <dirent.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/resource.h>


// To erase <<<--------------------------------------------------------------------------------
#include <bits/stdc++.h>

using namespace std;

#define FOR(x,b,e) for(int x = b; x < (e); ++x)
#define FOR2(x,b,e) for(int x = b; x <= (e); ++x)
#define FORD(x,b,e) for(int x = b; x >= (e); --x)
#define REP(x,n) for(int x = 0; x < (n); ++x)
#define REP2(x,n) for(int x = 0; x <= (n); ++x)
#define VAR(v,n) typeof(n) v = (n)
#define ALL(x) x.begin(), x.end()
#define ALL2(x) x.rbegin(), x.rend()
#define FOREACH(i,c) for(VAR(i, (c).begin()); i != (c).end(); ++i)
#define FOREACH2(i,c) for(VAR(i, (c).rbegin()); i != (c).rend(); ++i)
#define PB push_back
#define ST first
#define ND second
#define PF push_front
#define MP make_pair
#define I(x) static_cast<int>(x)
#define U(x) static_cast<unsigned>(x)
#define SZ(x) I((x).size())
#define eprint(...) fprintf(stderr, __VA_ARGS__)
#define ARR(t,x,n) t *x = new t[n]

typedef unsigned char uchar;
typedef long long LL;
typedef unsigned long long ULL;
typedef vector<int> VI;
typedef vector<VI> VVI;
typedef vector<LL> VLL;
typedef vector<string> VS;
typedef pair<int, int> PII;
typedef pair<LL, LL> PLLLL;
typedef vector<PII> VPII;
typedef vector<PLLLL> VPLLLL;
typedef vector<VPII> VVPII;

template<class T>
inline T abs(T x)
{return x<T() ? -x : x;}

template<class T, class T1>
void mini(T& a, const T1& x)
{if(x < a)a = x;}

template<class T, class T1>
void maxi(T& a, const T1& x)
{if(x > a)a = x;}

#define MOD(x, mod_val) {if(x >= mod_val) x %= mod_val; else if(x < 0){x %= mod_val;if(x < 0) x += mod_val;}}

#ifdef DEBUG

#define D(...) __VA_ARGS__
#define E(...) eprint(__VA_ARGS__)
#define LOG(x) cerr << #x << ": " << (x) << flush
#define LOGN(x) cerr << #x << ": " << (x) << endl

template<class C1, class C2>
ostream& operator<<(ostream& os, const pair<C1, C2>& _)
{return os << "(" << _.ST << "," << _.ND << ")";}

template<class T>
ostream& operator<<(ostream& os, const vector<T>& _)
{
	os << "{";
	if(_.size())
	{
		os << *_.begin();
		for(typename vector<T>::const_iterator i=++_.begin(); i!=_.end(); ++i)
			os << ", " << *i;
	}
	return os << "}";
}

template<class T>
ostream& operator<<(ostream& os, const deque<T>& _)
{
	os << "{";
	if(_.size())
	{
		os << *_.begin();
		for(typename deque<T>::const_iterator i=++_.begin(); i!=_.end(); ++i)
			os << ", " << *i;
	}
	return os << "}";
}

template<class T, class K>
ostream& operator<<(ostream& os, const set<T, K>& _)
{
	os << "{";
	if(_.size())
	{
		os << *_.begin();
		for(typename set<T>::const_iterator i=++_.begin(); i!=_.end(); ++i)
			os << ", " << *i;
	}
	return os << "}";
}

template<class T, class K>
ostream& operator<<(ostream& os, const map<T, K>& _)
{
	os << "{";
	if(_.size())
	{
		os << _.begin()->ST << " => " << _.begin()->ND;
		for(typename map<T, K>::const_iterator i=++_.begin(); i!=_.end(); ++i)
			os << ", " << i->ST << " => " << i->ND;
	}
	return os << "}";
}

template<class Iter>
void out(ostream& os, Iter begin, Iter end)
{
	os << "{";
	if(begin != end)
	{
		os << *begin;
		for(Iter i = ++begin; i != end; ++i)
			os << ", " << *i;
	}
	os << "}";
}

#define OUT(a, b) cerr << #a << ": ", out(cerr, a, b), eprint("\n")

#else
#define D(...)
#define E(...)
#define LOG(x)
#define LOGN(x)
#define OUT(...)
#endif
// To erase -------------------------------------------------------------------------------------------->>>

#define eprintf(args...) fprintf(stderr,args)

using std::cout;
using std::cerr;
using std::cin;
using std::endl;
using std::string;
using std::fstream;
using std::vector;
using Trie::CompressedTrie;

bool VERBOSE = false;

temporary_directory tmp_dir("conver.XXXXXX\0");

const int /*file_mod = 0644,*/ dir_mod = 0755;

string in_path, out_path, name, solution, problem_statement;
fstream config;

bool has_suffix(const string& str, const string& suffix) {
	return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

void findFilesInTmpDir(const string& path, vector<string>& v, const string& extension) {
		DIR *directory;
		dirent *current_file;
		if ((directory = opendir((in_path + path).c_str())))
			while ((current_file = readdir(directory)))
				if (strcmp(current_file->d_name, ".") && strcmp(current_file->d_name, "..") && has_suffix(current_file->d_name, extension))
					v.push_back(path + current_file->d_name);
}

struct empty {};

namespace sol{
	vector<string> solutions;

	void findSolutions() {
		solutions.clear();
		findFilesInTmpDir("sol/", solutions, ".cpp");
		findFilesInTmpDir("prog/", solutions, ".cpp");
	}

	void selectSolution() {
		if(solutions.empty()) {
			struct : std::exception {
			  const char* what() const throw() {return "There is no solutions to choose!\n";}
			} exc;
			throw exc;
		}
		if(solutions.size() == 1)
			solution = solutions[0];
		else {
			cout << "\nAvailable solutions:\n";
			for (size_t i = 0; i < solutions.size(); ++i)
				cout << i+1 << ". " << solutions[i] << endl;
			cout << "Which one would you choose? ";
			size_t x;
			for (;;) {
				if(!(cin >> x)) {
					cin.clear();
					cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
				}
				if (x-1 < solutions.size()) {
					solution = solutions[x-1];
					break;
				} else {
					cout << "Wrong nuber - try again: ";
				}
			}
		}
	}
} // namespace sol

namespace doc {
	vector<string> statements;

	void findStatements() {
		statements.clear();
		findFilesInTmpDir("doc/", statements, ".pdf");
		if (access((in_path + "statement.pdf").c_str(), F_OK) == 0)
			statements.push_back(in_path + "statement.pdf");
	}

	void selectStatement() {
		if(statements.empty()) {
			struct : std::exception {
			  const char* what() const throw() {return "There is no statement to choose!\n";}
			} exc;
			throw exc;
		}
		if(statements.size() == 1)
			problem_statement = statements[0];
		else {
			cout << "\nAvailable problem statements:\n";
			for (size_t i = 0; i < statements.size(); ++i)
				cout << i+1 << ". " << statements[i] << endl;
			cout << "Which one would you choose? ";
			size_t x;
			for (;;) {
				if(!(cin >> x)) {
					cin.clear();
					cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
				}
				if (x-1 < statements.size()) {
					problem_statement = statements[x-1];
					break;
				} else {
					cout << "Wrong nuber - try again: ";
				}
			}
		}
	}
} // namespace doc

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
			freopen("/dev/null", "w", stdout);
			freopen("/dev/null", "w", stderr);
			// if(chroot("chroot") != 0 || chdir("/") != 0 || (setgid((gid_t)1001) || setuid((uid_t)1001)))
				// _exit(-1);

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
		if (VERBOSE)
			eprint("\tReturned %i\ttime: %.6lf s\n", r_val, cl/1000000.0);
		if(r_val == 0)
		{
/*			// Set right handler
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
				// if(chroot("..") != 0 || chdir("/judge") != 0 || (setgid((gid_t)1001) || setuid((uid_t)1001)))
					// _exit(-1);

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
				if(WEXITSTATUS(status) == 0)*/
					res_stat = RES_OK;
/*				else if(WEXITSTATUS(status) == 1)
					res_stat = RES_WA;
				else
					res_stat = RES_EVF;
				D(status = WEXITSTATUS(status);)
			}
			else
				D(status = WTERMSIG(status) + 128,)
				res_stat = RES_EVF;
			D(eprint("\tChecker returned: %i\ttime: %.6lf s\n", status, 20.0 - timer.it_value.tv_sec - timer.it_value.tv_usec/1000000.0);)*/

		}
		else if(cl == time_limit)
			res_stat = RES_TL;
		else
			res_stat = RES_RTE;
		return 0;
	}
}

namespace tests {
	vector<pair<string, string> > tests;

	pair<string, string> _getId(const string& str) {
		pair<string, string> res;
		string::const_reverse_iterator i = str.rbegin();
		while(i != str.rend() && isalpha(*i))
			res.second += *i++;
		while(i != str.rend() && isdigit(*i))
			res.first += *i++;
		std::reverse(res.first.begin(), res.first.end());
		std::reverse(res.second.begin(), res.second.end());
		return res;
	}

	string getId(const string& str) {
		pair<string, string> x = _getId(str);
		return x.first + x.second;
	}

	bool comparator(const pair<string, string>& a, const pair<string, string>& b) {
		pair<string, string> x = _getId(a.first), y = _getId(b.first);
		return (x.first.size() == y.first.size() ? (x.first == y.first ? x.second < y.second : x.first < y.first) : x.first.size() < y.first.size());
	}

	void findTests() {
		tests.clear();
		vector<string> in, out;
		findFilesInTmpDir("in/", in, ".in");
		findFilesInTmpDir("tests/", in, ".in");
		findFilesInTmpDir("out/", out, ".out");
		findFilesInTmpDir("tests/", out, ".out");
		CompressedTrie<string*> trie;
		for (vector<string>::iterator i = in.begin(); i != in.end(); ++i) {
			i->erase(i->end()-3, i->end());
			string x = *i;
			x.erase(0, x.find("/"));
			*trie.insert(x).ST = &*i;
		}
		for (vector<string>::iterator i = out.begin(); i != out.end(); ++i) {
			i->erase(i->end()-4, i->end());
			string x = *i;
			x.erase(0, x.find("/"));
			typeof(trie.end()) it;
			if((it = trie.find(x)) != trie.end())
				tests.push_back(MP(**it, *i));
		}
		sort(tests.begin(), tests.end(), comparator);
		LOGN(tests);
	}

	void moveTests(){
		for (vector<pair<string,string> >::iterator i = tests.begin(); i != tests.end(); ++i) {
			rename((in_path + i->ST + ".in").c_str(),(out_path + "tests/" + name + getId(i->ST) + ".in").c_str());
			rename((in_path + i->ND + ".out").c_str(),(out_path + "tests/" + name + getId(i->ND) + ".out").c_str());
		}
	}


	void run_on_test(const string& test) {
		// Setup runtime variables
		string tmp = out_path+"tests/"+test+".in";
		runtime::input = new char[tmp.size()+1];
		strcpy(runtime::input, tmp.c_str());

		tmp = out_path+"tests/"+test+".out";
		runtime::output = new char[tmp.size()+1];
		strcpy(runtime::output, tmp.c_str());

		// runtime::answer = const_cast<char*>(outf_name.c_str());

		tmp = string(tmp_dir) + "exec";
		runtime::exec = new char[tmp.size()+1];
		strcpy(runtime::exec, tmp.c_str());

		// runtime::checker = const_cast<char*>(checker.c_str());

		// Convert memory limit format from string (KB) to long long (B)
		// runtime::memory_limit = 0;
		// for(int len = memory_limit.size(), i = 0; i < len; ++i)
		// 	runtime::memory_limit *= 10,
		// 	runtime::memory_limit += memory_limit[i] - '0';
		// runtime::memory_limit *= 1024;

		// Time limit
		runtime::time_limit = 20.0 * 1000000;

		// Runtime
		if (VERBOSE)
			eprint("\nRunning on: %s\tlimits -> (TIME: %lf s, MEM: %lli KB)\n", test.c_str(), runtime::time_limit/1000000.0, runtime::memory_limit >> 10);
		pid_t pid = clone(runtime::run, (char*)malloc(128 * 1024 * 1024) + 128 * 1024 * 1024, CLONE_VM | SIGCHLD, NULL);
		waitpid(pid, NULL, 0);

		// Free resources
		delete[] runtime::input;
		delete[] runtime::output;
		delete[] runtime::exec;

		tmp = myto_string(ceil(runtime::cl/10000)*4);
		tmp.insert(0, 3-tmp.size(), '0');
		if(*tmp.rbegin() == '0' && *++tmp.rbegin() == '0')
			tmp.erase(tmp.end()-2, tmp.end());
		else {
			tmp.insert(tmp.size()-2, 1, '.');
			while(*tmp.rbegin() == '0')
				tmp.erase(tmp.end()-1, tmp.end());
		}
		if(tmp == "0" || tmp == "0.01")
			tmp = "0.02";
		// It has to be here if not using instrumentation like Pintool
		if (tmp < "0.10")
			tmp = "0.10";

		printf("\t%.2lf / %s", runtime::cl/10000/100.0, tmp.c_str());
		config << tmp;

		switch(runtime::res_stat)
		{
			case runtime::RES_OK:
				printf("\tStatus: \033[1;32mOK\033[0m\n");
				break;
			case runtime::RES_WA:
				printf("\tStatus: \033[1;31mWrong answer\033[0m\n");
				break;
			case runtime::RES_TL:
				printf("\tStatus: \033[1;33mTime limit\033[0m\n");
				break;
			case runtime::RES_RTE:
				printf("\tStatus: \033[1;33mRuntime error\033[0m\n");
				break;
			case runtime::RES_EVF:
				printf("\tStatus: \033[1;31mEvaluation failure\033[0m\n");
		}
	}

	void setLimits(unsigned long long memory_limit) {
		runtime::memory_limit = memory_limit << 10;
		size_t begin = 0, end = 0, groups = _getId(tests[0].first).first != "0";
		long long points = 100;
		while (++begin < tests.size())
			if (_getId(tests[begin].first).first != "0" && _getId(tests[begin - 1].first).first != _getId(tests[begin].first).first)
				++groups;
		LOGN(groups);
		begin = 0;
		while (begin < tests.size()) {
			string test_name;
			pair<string, string> id = _getId(tests[begin].first);

			while(++end < tests.size() && id.first == _getId(tests[end].first).first) {}

			test_name = name + id.first + id.second;
			config << test_name << ' ';
			run_on_test(test_name);
			config << ' ' << end - begin << ' ' << (id.first != "0" ? points / groups : 0) << endl;

			if(id.first != "0")
				points -= points / groups--;

			while(++begin < end) {
				test_name = name + id.first + _getId(tests[begin].first).second;
				config << test_name << ' ';
				run_on_test(test_name);
				config << endl;
			}
		}
	}
} // namespace tests

void parseOptions(int& argc, char **argv) {
	int new_argc = 0;
	for (int i = 0; i < argc; ++i) {
		if (argv[i][0] == '-') {
			if (0 == strcmp(argv[i], "-v") || 0 == strcmp(argv[i], "--verbose"))
				VERBOSE = true;
			else if (0 == strcmp(argv[i], "-n") && i + 1 < argc)
				name = argv[++i];
			else if (0 == comparePrefix(argv[i], "--name="))
				name = string(argv[i]).substr(7);
			else
				eprintf("Unknown argument: '%s'\n", argv[i]);
		} else
			argv[new_argc++] = argv[i];
	}
	argc = new_argc;
}

int main(int argc, char **argv) {
	parseOptions(argc, argv);

	if(argc < 3) {
		eprintf("Usage: conver [options] task_package out_package [mem_limit] [checker]\n\nOptions:\n  -v, --verbose		Verbose mode\n  -n NAME, --name=NAME		Set task name to NAME\n\ntask_package and out_package have to be .zip or directory\nmem_limit is in kB\n");
		return 1;
	}

	// signal control
	/*signal(SIGHUP, exit);
	signal(SIGINT, exit);
	signal(SIGQUIT, exit);
	signal(SIGILL, exit);
	signal(SIGTRAP, exit);
	signal(SIGABRT, exit);
	signal(SIGIOT, exit);
	signal(SIGBUS, exit);
	signal(SIGFPE, exit);
	signal(SIGUSR1, exit);
	signal(SIGSEGV, exit);
	signal(SIGUSR2, exit);
	signal(SIGPIPE, exit);
	signal(SIGALRM, exit);
	signal(SIGTERM, exit);
	signal(SIGSTKFLT, exit);
	signal(_NSIG, exit);*/
	// signal(SIGKILL, exit); // We won't block SIGKILL

	in_path = argv[1];
	if(in_path.size() > 3 && 0 == in_path.compare(in_path.size()-4, 4, ".zip")) {
		int ret = system((string("unzip ") << argv[1] << " -d " << tmp_dir).c_str());
		if(0 != ret)
			return ret;
		in_path.erase(in_path.size()-4, 4);
	} else {
		int ret = system(("cp -rf " << in_path << " " << tmp_dir).c_str());
		if(0 != ret)
			return ret;
	}
	unsigned long long mem_limit = 131072;
	string checker = "default";
	if(argc > 3)
		sscanf(argv[3], "%llu", &mem_limit);
	if(argc > 4)
		checker = argv[4];
	if (name.empty()) // If not set with options
		name = in_path;
	in_path = tmp_dir.name() + in_path;
	out_path = in_path + "1/";
	in_path << "/";
	D(cerr << in_path << endl << out_path << endl;)
	mkdir(out_path.c_str(), dir_mod);
	mkdir((out_path + "prog/").c_str(), dir_mod);
	config.open((out_path+"conf.cfg").c_str(), std::ios::out);
	if(!config.good()) {
		eprintf("Cannot create config file\n");
		return 1;
	}
	config << tolower(name.size() < 4 ? name : name.substr(0, 3)) << "\n" << name << "\n" << checker << "\n" << mem_limit << endl;
	doc::findStatements();
	doc::selectStatement();
	sol::findSolutions();
	sol::selectSolution();
	cerr << "Task name: '" << name << "'\n" << "Problem statement: " << problem_statement << "\nSolution: " << solution << endl;
	rename((in_path + problem_statement).c_str(), (out_path + "statement.pdf").c_str());
	rename((in_path + solution).c_str(), (out_path + "prog/" + name + ".cpp").c_str());
	cerr << "Compile: " << solution << endl;
	if(0 != compile(out_path + "prog/" + name + ".cpp", tmp_dir.sname() + "exec")) {
		eprintf("Solution compilation failed:\n");
		cerr << compile.getErrors() << endl;
		return 2;
	}
	else {
		cerr << "Success!" << endl;
	}
	tests::findTests();
	mkdir((out_path + "tests/").c_str(), dir_mod);
	tests::moveTests();
	tests::setLimits(mem_limit);
	config.close();
	remove_r(in_path.c_str());
	string result = argv[2];
	if(result.size() > 3 && 0 == result.compare(result.size()-4, 4, ".zip")) {
		result.erase(result.size()-4, 4);
		int ret = system((string("mv ") << out_path << " " << tmp_dir << result << "; (cd " << tmp_dir << "; zip -r ../" << result << ".zip " << result << ")").c_str());
		D(cerr << (string("mv ") << out_path << " " << tmp_dir << result << "; (cd " << tmp_dir << "; zip -r ../" << result << ".zip " << result << ")") << endl;)
		if(0 != ret)
			return ret;
	} else {
		int ret = system(("rm -rf " << result << "; mv " << out_path << " " << result).c_str());
		D(cerr << ("rm -rf " << result << "; mv " << out_path << " " << result) << endl;)
		if(0 != ret)
			return ret;
	}
	return 0;
}

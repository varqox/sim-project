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
#include <unistd.h>
#include <linux/limits.h> // PATH_MAX
#include <sys/shm.h>

#define ST first
#define ND second

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

temporary_directory tmp_dir("conver.XXXXXX");

const int /*file_mod = 0644,*/ dir_mod = 0755;

string in_path, out_path, name, tag_name, solution, problem_statement;
fstream config;

bool has_suffix(const string& str, const string& suffix) {
	return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

void findFilesInTmpDir(const string& path, vector<string>& v, const string& extension) {
		DIR *directory;
		dirent *current_file;
		if ((directory = opendir((in_path + path).c_str()))) {
			while ((current_file = readdir(directory)))
				if (strcmp(current_file->d_name, ".") && strcmp(current_file->d_name, "..") && has_suffix(current_file->d_name, extension))
					v.push_back(path + current_file->d_name);
			closedir(directory);
		}
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
			statements.push_back("statement.pdf");
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

struct RuntimeInfo {
	enum ResultStatusSet {RES_OK, RES_WA, RES_TL, RES_RTE, RES_EVF};

	char in_file[PATH_MAX], out_file[PATH_MAX], exec[PATH_MAX];
	long long time_limit, memory_limit;
	//        ^ in usec   ^ in bytes
	ResultStatusSet res_stat;

	RuntimeInfo(const string& in, const string& out, const string& ec,
			long long tl, long long ml,	ResultStatusSet rs = RES_OK)
		: time_limit(tl), memory_limit(ml), res_stat(rs) {
			strcpy(in_file, in.c_str());
			strcpy(out_file, out.c_str());
			strcpy(exec, ec.c_str());
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
			// Set up enviroment
			freopen(rt_stat->in_file, "r", stdin);
			freopen("/dev/null", "w", stdout);
			freopen("/dev/null", "w", stderr);
			// if(chroot("chroot") != 0 || chdir("/") != 0 || (setgid((gid_t)1001) || setuid((uid_t)1001)))
				// _exit(-1);

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
		else if(WIFSIGNALED(r_val))
			r_val = WTERMSIG(r_val) + 128;
		else
			// Shouldn't happen. Unknown status...
			r_val = EXIT_FAILURE;
		if (VERBOSE)
			eprint("\tReturned %i\ttime: %.6lf s\n", r_val, cl/1000000.0);

		if(r_val == 0)
					rt_stat->res_stat = RuntimeInfo::RES_OK;
		else if(cl == rt_stat->time_limit)
			rt_stat->res_stat = RuntimeInfo::RES_TL;
		else
			rt_stat->res_stat = RuntimeInfo::RES_RTE;
		rt_stat->time_limit = cl;
		shmdt(rt_stat); // Detach shared memory segment
	}
}

class SharedMemorySegment {
private:
	int id_;
	void* addr_;
	SharedMemorySegment(const SharedMemorySegment&);
	SharedMemorySegment& operator=(const SharedMemorySegment&);

public:
	SharedMemorySegment(size_t size): id_(shmget(IPC_PRIVATE, size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR)), addr_(NULL) {
		if (id_ != -1) {
			if ((addr_ = shmat(id_, NULL, 0)) == (void*)-1)
				addr_ = NULL;
			shmctl(id_, IPC_RMID, NULL);
		}
	}

	~SharedMemorySegment() {
		if (addr_)
			shmdt(addr_);
	}

	int key() const { return id_; }

	void* addr() const { return addr_; }
};

namespace tests {
	vector<pair<string, string> > tests;

	struct TestId {
		string ST, ND;
		TestId(): ST(), ND() {}
	};

	// Extract test id e.g. "test1abc" -> ("1", "abc")
	TestId _getId(const string& str) {
		TestId res;
		string::const_reverse_iterator i = str.rbegin();
		while(i != str.rend() && isalpha(*i))
			res.ND += *i++;
		while(i != str.rend() && isdigit(*i))
			res.ST += *i++;
		std::reverse(res.ST.begin(), res.ST.end());
		std::reverse(res.ND.begin(), res.ND.end());
		return res;
	}

	// Extract test id e.g. "test1abc" -> "1abc"
	inline string getId(const TestId& id) {
		return id.ST + id.ND;
	}

	inline string getId(const string& str) {
		return getId(_getId(str));
	}

	bool comparator(const pair<string, string>& a, const pair<string, string>& b) {
		TestId x = _getId(a.ST), y = _getId(b.ST);
		return (x.ST.size() == y.ST.size() ? (x.ST == y.ST ? x.ND < y.ND : x.ST < y.ST) : x.ST.size() < y.ST.size());
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
			rename((in_path + i->ST + ".in").c_str(),(out_path + "tests/" + tag_name + getId(i->ST) + ".in").c_str());
			rename((in_path + i->ND + ".out").c_str(),(out_path + "tests/" + tag_name + getId(i->ND) + ".out").c_str());
		}
	}


	void run_on_test(const string& test, long long mem_limit) {
		// Create shared memory segment (only once)
		static SharedMemorySegment shm_sgmt(sizeof(RuntimeInfo));
		if (shm_sgmt.addr() == NULL) {
			eprintf("Failed to get shared memory segment\n");
			return;
		}
		// Setup runtime variables (time_limit: 20s)
		RuntimeInfo *rt_stat = new (shm_sgmt.addr()) RuntimeInfo(out_path + "tests/" + test + ".in", out_path + "tests/" + test + ".out", tmp_dir.sname() + "exec", 20 * 1000000, mem_limit << 10);
		// Runtime
		if (VERBOSE)
			eprint("\nRunning on: %s\tlimits -> (TIME: %lf s, MEM: %lli KB)\n", test.c_str(), rt_stat->time_limit/1000000.0, rt_stat->memory_limit);

		pid_t cpid = fork();
		if (cpid == 0) {
			runtime::run(shm_sgmt.key());
			_exit(0);
		} else if (cpid == -1) {
			eprintf("%s: filed to fork()\n", __PRETTY_FUNCTION__);
			abort();
		}
		waitpid(cpid, NULL, 0);

		string tmp = myto_string(rt_stat->time_limit/(10000/4));
		if (tmp.size() < 3)
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

		if (VERBOSE)
			printf("\t%.2lf / %s", rt_stat->time_limit/10000/100.0, tmp.c_str());
		else
			printf("  %-15s%.2lf / %s", test.c_str(), rt_stat->time_limit/10000/100.0, tmp.c_str());
		config << tmp;

		switch(rt_stat->res_stat)
		{
			case RuntimeInfo::RES_OK:
				printf("\tStatus: \033[1;32mOK\033[0m\n");
				break;
			case RuntimeInfo::RES_WA:
				printf("\tStatus: \033[1;31mWrong answer\033[0m\n");
				break;
			case RuntimeInfo::RES_TL:
				printf("\tStatus: \033[1;33mTime limit\033[0m\n");
				break;
			case RuntimeInfo::RES_RTE:
				printf("\tStatus: \033[1;33mRuntime error\033[0m\n");
				break;
			case RuntimeInfo::RES_EVF:
				printf("\tStatus: \033[1;31mEvaluation failure\033[0m\n");
		}
	}

	inline bool is0PointTest(const TestId& id) {
		return (id.ST == "0" || id.ND == "ocen");
	}

	void setLimits(long long memory_limit) {
		TestId last_id = _getId(tests[0].ST), current_id; // last_id have to contain non-0 points test
		vector<TestId> initial_ids, rest_ids; // Initial tests are 0-points tests
		size_t begin = 0, end = 0, groups = 0;
		if (is0PointTest(last_id)) { // We have to set an impossible value
			initial_ids.push_back(last_id);
			last_id.ST = "";
		} else {
			rest_ids.push_back(last_id);
			groups = 1;
		}
		// groups is numer of tests groups for which you get non-0 points
		long long points = 100;
		while (++begin < tests.size()) {
			current_id = _getId(tests[begin].ST);
			if (!is0PointTest(current_id)) {
				rest_ids.push_back(current_id);
				if (last_id.ST != current_id.ST)
					++groups;
				last_id = current_id;
			} else
				initial_ids.push_back(current_id);
		}
		LOGN(groups);
		string test_name; // Name in output directory
		// Take care of initial tests (set limits)
		test_name = tag_name + getId(initial_ids[begin = 0]);
		config << test_name << ' ';
		run_on_test(test_name, memory_limit);
		config << ' ' << initial_ids.size() << " 0\n";
		while (++begin < initial_ids.size()) {
			test_name = tag_name + getId(initial_ids[begin]);
			config << test_name << ' ';
			run_on_test(test_name, memory_limit);
			config << endl;
		}
		// Now set limits for each non-0 points group of tests
		begin = 0;
		while (begin < rest_ids.size()) {
			// Move end until [begin, end) contain whole group of tests
			while (++end < rest_ids.size() && rest_ids[begin].ST == rest_ids[end].ST) {}

			// Run on first test and manage group config
			test_name = tag_name + getId(rest_ids[begin]);
			config << test_name << ' ';
			run_on_test(test_name, memory_limit);
			config << ' ' << end - begin << ' ' << points / groups << endl;

			points -= points / groups--;

			while (++begin < end) {
				test_name = tag_name + getId(rest_ids[begin]);
				config << test_name << ' ';
				run_on_test(test_name, memory_limit);
				config << endl;
			}
		}
	}
} // namespace tests

long long memory_limit = 1 << 17; // in kb -> 128MB
bool ZIP = false;
string checker = "default", target_path = "./";

void help() {
	eprintf("Usage: conver [options] problem_package\n");
	eprintf("Convert problem_package to SIM package.\n");
	eprintf("  problem_package have to be .zip or directory\n\n");
	eprintf("Options:\n");
	eprintf("  -z, --zip                                  Zip final package\n");
	eprintf("  -v, --verbose                              Verbose mode\n");
	eprintf("  -n NAME, --name=NAME                       Set problem name to NAME\n");
	eprintf("  -o OUT_DIR                                 Set target directory to which done package will be moved\n");
	eprintf("  -m MEM_LIMIT, --memory-limit=MEM_LIMIT     Set problem memory limit to MEM_LIMIT in kB\n");
	eprintf("  --checker=CHECKER                          Set problem checker to CHECKER\n");
}

void parseOptions(int& argc, char **argv) {
	int new_argc = 0;
	for (int i = 0; i < argc; ++i) {
		if (argv[i][0] == '-') {
			if (0 == strcmp(argv[i], "-v") || 0 == strcmp(argv[i], "--verbose"))
				VERBOSE = true;
			else if (0 == strcmp(argv[i], "-z") || 0 == strcmp(argv[i], "--zip"))
				ZIP = true;
			else if (0 == strcmp(argv[i], "-h") || 0 == strcmp(argv[i], "--help")) {
				help();
				exit(0);
			} else if (0 == strcmp(argv[i], "-n") && i + 1 < argc)
				name = argv[++i];
			else if (isPrefix(argv[i], "--name="))
				name = string(argv[i]).substr(7);
			else if (0 == strcmp(argv[i], "-o") && i + 1 < argc)
				target_path = argv[++i];
			else if (0 == strcmp(argv[i], "-m") && i + 1 < argc) {
				if (1 > sscanf(argv[++i], "%lli", &memory_limit))
					eprintf("Wrong memory limit format\n");
			} else if (isPrefix(argv[i], "--memory-limit=")) {
				if (1 > sscanf(argv[i] + 15, "%lli", &memory_limit))
					eprintf("Wrong memory limit format\n");
			} else if (isPrefix(argv[i], "--checker="))
				checker = string(argv[i]).substr(11);
			else
				eprintf("Unknown argument: '%s'\n", argv[i]);
		} else
			argv[new_argc++] = argv[i];
	}
	argc = new_argc;
}

int main(int argc, char **argv) {
	parseOptions(argc, argv);

	if(argc < 2 ) {
		help();
		return 1;
	}

	// Signal control
	struct sigaction sa;
	memset (&sa, 0, sizeof(sa));
	sa.sa_handler = &exit;

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

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
	tag_name = tolower(name.size() < 4 ? name : name.substr(0, 3));
	config << tag_name << "\n" << name << "\n" << checker << "\n" << memory_limit << endl;
	doc::findStatements();
	doc::selectStatement();
	sol::findSolutions();
	sol::selectSolution();
	cerr << "Problem name: '" << name << "'\nTag name: '" << tag_name << "'\nProblem statement: " << problem_statement << "\nSolution: " << solution << endl;
	if(0 != rename((in_path + problem_statement).c_str(), (out_path + "statement.pdf").c_str()) )
		cerr << "Error moving: " << in_path << problem_statement << " -> " << out_path << "statement.pdf" << endl;
	if(0 != rename((in_path + solution).c_str(), (out_path + "prog/" + tag_name + ".cpp").c_str()) )
		cerr << "Error moving: " << in_path << solution << " -> " << out_path << "prog/" << tag_name << ".cpp" << endl;
	cerr << "Compile: " << out_path << "prog/" << tag_name << ".cpp" << endl;
	if(0 != compile(out_path + "prog/" + tag_name + ".cpp", tmp_dir.sname() + "exec")) {
		eprintf("Solution compilation failed:\n");
		cerr << compile.getErrors() << endl;
		return 2;
	}
	else
		cerr << "Succeeded!" << endl;
	tests::findTests();
	mkdir((out_path + "tests/").c_str(), dir_mod);
	tests::moveTests();
	tests::setLimits(memory_limit);
	config.close();
	remove_r(in_path.c_str());

	if (*--target_path.end() != '/')
		target_path += '/';
	if(ZIP) {
		if (VERBOSE)
			cerr << "zip: '" << out_path << "' -> '" << out_path << tmp_dir << tag_name << ".zip'\n";
		pid_t cpid = fork();
		if (cpid == 0) {
			execlp("zip", "zip", "-r", (char*)(tmp_dir.sname() + tag_name + ".zip").c_str(), (char*)out_path.c_str(), NULL);
			_exit(-1);
		} else if (cpid == -1)
			eprintf("fork() failed\n");
		int r;
		waitpid(cpid, &r, 0);
		if (r) {
			cerr << "Error zip: '" << out_path << "' -> '" << out_path << tmp_dir << tag_name << ".zip'\n";
			D(abort();)
			return 1;
		}
		if (VERBOSE)
			cerr << "Moving: '" << out_path << out_path << tmp_dir << tag_name << ".zip'" << "' -> '" << target_path << tag_name << ".zip'\n";
		if (0 != rename((tmp_dir.sname() + tag_name + ".zip").c_str(), (target_path + tag_name + ".zip").c_str())) {
			cerr << "Error moving: '" << out_path << out_path << tmp_dir << tag_name << ".zip'" << "' -> '" << target_path << tag_name << ".zip'\n";
			D(abort();)
			return 1;
		}
	} else {
		if (VERBOSE)
			cerr << "Moving: '" << out_path << "' -> '" << target_path << tag_name << "'\n";
		if (0 != rename(out_path.c_str(), (target_path + tag_name).c_str())) {
			cerr << "Error moving: '" << out_path << "' -> '" << target_path << tag_name << "'\n";
			D(abort();)
			return 1;
		}
	}
	return 0;
}

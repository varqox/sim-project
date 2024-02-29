// Krzysztof Małysa
#include <cmath>
#include <csignal>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>

using namespace std;

static inline int ignore_sigpipe() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = SA_RESTART;
    return sigaction(SIGPIPE, &sa, NULL);
}

#ifdef NDEBUG
# define my_assert(...) ((void)0)
#else
# define my_assert(expr) \
  ((expr) ? (void)0 : (fprintf(stderr, "%s:%i %s: Assertion `%s' failed.\n", \
    __FILE__, __LINE__, __PRETTY_FUNCTION__, #expr), exit(-1)))
#endif

inline void checker_ok() {
	cerr << "OK\n";
	exit(0);
}

template<class Arg1, class... Args>
inline std::enable_if_t<not std::is_convertible<Arg1, double>::value, void>
checker_ok(Arg1&& message_p1, Args&&... message_p2) {
	cerr << "OK\n\n" << std::forward<Arg1>(message_p1);
	(void)initializer_list<int>{(cerr << std::forward<Args>(message_p2), 0)...};
	cerr << '\n';
	exit(0);
}

template<class... Args>
inline void checker_ok(double score, Args&&... message) {
	cerr << "OK\n" << score << '\n';
	(void)initializer_list<int>{(cerr << std::forward<Args>(message), 0)...};
	cerr << '\n';
	exit(0);
}

template<class... Args>
inline void checker_wrong(Args&&... message) {
	cerr << "WRONG\n\n";
	(void)initializer_list<int>{(cerr << std::forward<Args>(message), 0)...};
	cerr << '\n';
	exit(0);
}

constexpr char space = ' ', newline = '\n';
struct EofType {} eof;

template<class T>
struct Integer {
	T& val;
	T min_val, max_val;

	Integer(T& v, T mnv, T mxv) : val(v), min_val(mnv), max_val(mxv) {}
};

template<class T, class T1, class T2>
inline Integer<T> integer(T& val, T1 min_val, T2 max_val) noexcept {
	my_assert(min_val >= std::numeric_limits<T>::min());
	my_assert(max_val <= std::numeric_limits<T>::max());
	return {val, static_cast<T>(min_val), static_cast<T>(max_val)};
}

class Scanner {
	FILE* file_;
	size_t line_ = 1;
	bool eofed = false;

	bool getchar(int& ch) noexcept {
		if (eofed)
			return false;

		ch = fgetc_unlocked(file_);
		eofed = (ch == EOF);
		line_ += (ch == '\n');
		return (not eofed);
	}

	void ungetchar(int ch) noexcept {
		ungetc(ch, file_);
		line_ -= (ch == '\n');
	}

	template<class... Args>
	void fatal(Args&&... message) {
		checker_wrong("Line ", line_, ": ", std::forward<Args>(message)...);
	}

	static string char_description(int ch) {
		if (isgraph(ch))
			return {'\'', (char)ch, '\''};

		if (ch == ' ')
			return "space";
		if (ch == '\n')
			return "'\\n'";
		if (ch == '\r')
			return "'\\r'";
		if (ch == '\t')
			return "'\\t'";
		if (ch == '\0')
			return "'\\0'";

		constexpr char digits[] = "0123456789abcdef";
		return {'\'', '\\', 'x', digits[ch >> 4], digits[ch & 15], '\''};
	}

	Scanner& scan_char(char exp_ch) {
		int ch;
		if (not getchar(ch))
			fatal("Read EOF, expected ", char_description(ch));
		if (ch != exp_ch) {
			fatal("Read ", char_description(ch), ", expected ",
			      char_description(exp_ch));
		}

		return *this;
	}

	static bool is_digit(int ch) noexcept { return ('0' <= ch and ch <= '9'); }

	template<class T>
	void scan_integer(T& val) {
		int ch;
		if (not getchar(ch))
			fatal("Read EOF, expected a number");

		bool minus = false;
		if (not std::is_unsigned<T>::value and ch == '-') {
			minus = true;
			if (not getchar(ch))
				fatal("Read EOF, expected a number");
		}

		if (not is_digit(ch))
			fatal("Read ", char_description(ch), ", expected a number");

		val = (minus ? '0' - ch : ch - '0'); // Will not overflow
		for (;;) {
			if (not getchar(ch))
				break;

			if (not isdigit(ch)) {
				ungetchar(ch);
				break;
			}

			if (__builtin_mul_overflow(val, 10, &val))
				fatal("Integer value out of range");

			if (not minus and __builtin_add_overflow(val, ch - '0', &val))
				fatal("Integer value out of range");

			if (minus and __builtin_sub_overflow(val, ch - '0', &val))
				fatal("Integer value out of range");
		}
	}

public:
	explicit Scanner(FILE* file) : file_(file) {}

	Scanner& operator>>(const char& c) { return scan_char(c); }

	Scanner& operator>>(char& c) {
		int ch;
		if (not getchar(ch))
			fatal("Read EOF, expected a character");

		c = ch;
		return *this;
	}

	Scanner& operator>>(EofType) {
		int ch;
		if (getchar(ch))
			fatal("Read ", char_description(ch), ", expected EOF");

		return *this;
	}

	// Does not omit white-spaces
	Scanner& operator>>(string& str) {
		int ch;
		str.clear();
		while (getchar(ch)) {
			if (isspace(ch)) {
				ungetchar(ch);
				break;
			}

			str += ch;
		}

		return *this;
	}

	template<class T>
	Scanner& operator>>(Integer<T>&& integer) {
		scan_integer(integer.val);
		if (integer.val < integer.min_val)
			fatal("Integer ", integer.val, " is too small (smaller than ", integer.min_val, ')');
		if (integer.val > integer.max_val)
			fatal("Integer ", integer.val, " is too big (bigger than ", integer.max_val, ')');

		return *this;
	}
};

int main(int argc, char** argv) {
	// argv[0] command (ignored)
	// argv[1] test_in
	//
	// stdin = stdout of solution
	// stdout = stdin of solution
	// stderr = where to write checker output
	//
	// Output:
	// Line 1: "OK" or "WRONG"
	// Line 2 (optional; ignored if line 1 == "WRONG" - score is set to 0 anyway):
	//   leave empty or provide a real number x from interval [0, 100], which
	//   means that the test will get no more than x percent of its maximal score.
	// Line 3 and next (optional): A checker comment

	ignore_sigpipe(); // When checker waits for input but solution exits a
	                  // SIGPIPE signal is delivered - we have to ignore it

	my_assert(argc == 2);

	Scanner scan(stdin);
	ifstream test(argv[1]);

	int n;
	long long k;
	test >> n >> k;

	vector<long long> v(n);
	for (auto& x : v)
		test >> x;

	int limit = (int)log2(n) + 1;
	int queries = 0;

	cout << n << ' ' << k << newline << flush;
	for (;;) {
		char c;
		int x;
		scan >> c >> space >> integer(x, 1, n) >> newline;
		--x;
		if (c == '!') {
			scan >> eof;
			if (v[x] == k) {
				// [0, limit] gives 100%, then it drops linearly to 0 and for [2 * limit, inf) it gives 0
				checker_ok(100 * min(1., max(0., 1 - (queries - limit) / (double)limit)),
				           "Used ", queries, " queries out of ", limit);
			} else {
				checker_wrong("Invalid answer");
			}
		}

		if (c != '?')
			checker_wrong("Invalid command: ", c);

		cout << v[x] << newline << flush;
		++queries;
	}

	// Remember to check for EOF upon successful exit
	return 0;
}

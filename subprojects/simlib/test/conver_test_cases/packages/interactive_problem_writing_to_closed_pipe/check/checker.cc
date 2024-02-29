#include <csignal>
#include <cstring>
#include <iostream>
#include <type_traits>
#include <unistd.h>
#include <variant>

using namespace std;

#ifdef NDEBUG
#define my_assert(...) ((void)0)
#else
#define my_assert(expr)                                                        \
	((expr) ? (void)0                                                          \
	        : (fprintf(stderr, "%s:%i %s: Assertion `%s' failed.\n", __FILE__, \
	                   __LINE__, __PRETTY_FUNCTION__, #expr),                  \
	           exit(1)))
#endif

struct Verdict {
	ostream& out = cerr;

	struct OK {
		double score;
		std::string message_prefix;
	};

	struct WRONG {
		std::string message_prefix;
	};

	// E.g. usage:
	// ...
	// // Give partial points, even if the rest of the answer will be invalid:
	// verdict.wrong_override = Verdict::OK {50, "First line is correct\n"};
	std::variant<OK, WRONG> wrong_override = WRONG {};

	// 100% points
	[[noreturn]] void ok() {
		out << "OK" << endl;
		exit(0);
	}

	// @p score * 1% points
	template <class... Args>
	[[noreturn]] void ok(double score, Args&&... message) {
		out << "OK\n" << score << '\n';
		(out << ... << std::forward<Args>(message)) << endl;
		exit(0);
	}

	// 100% points
	template <class Arg1, class... Args,
	          enable_if_t<not is_convertible_v<Arg1, double>, int> = 0>
	[[noreturn]] void ok(Arg1&& message_part1, Args&&... message_part2) {
		out << "OK\n\n" << std::forward<Arg1>(message_part1);
		(out << ... << std::forward<Args>(message_part2)) << endl;
		exit(0);
	}

	// If wrong_override holds OK, then wrong_override.score * 1% points, 0%
	// points otherwise
	template <class... Args>
	[[noreturn]] void wrong(Args&&... message) {
		std::visit(
		   [&](auto&& wr) {
			   using T = decay_t<decltype(wr)>;
			   if constexpr (is_same_v<T, OK>) {
				   out << "OK\n" << wr.score << "\n";
			   } else if constexpr (is_same_v<T, WRONG>) {
				   out << "WRONG\n\n";
			   }
			   out << wr.message_prefix;
		   },
		   wrong_override);

		(out << ... << std::forward<Args>(message)) << endl;
		exit(0);
	}
} verdict;

static inline int __attribute__((constructor)) ignore_sigpipe() {
	//                  ^ Trick to make compiler call it before main()
	// When checker waits for input but solution exits, a SIGPIPE signal is
	// delivered - we have to ignore it, otherwise it would kill checker process
	struct sigaction sa;
	std::memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = SA_RESTART;
	return sigaction(SIGPIPE, &sa, nullptr);
}

#include <chrono>

int main(int argc, char** argv) {
	// argv[0] command (ignored)
	// argv[1] test_in
	//
	// stdin = stdout of solution
	// stdout = stdin of solution
	// stderr = where to write checker output
	//
	// Output (to stderr):
	// Line 1: "OK" or "WRONG"
	// Line 2 (optional; ignored if line 1 == "WRONG" - score is set to 0
	// anyway):
	//   leave empty or provide a real number x from interval [0, 100], which
	//   means that the test will get no more than x percent of its maximal
	//   score.
	// Line 3 and next (optional): A checker comment

	my_assert(argc == 2);

	// Well implemented judger will hold checker file descriptors to distinguish
	// solution errors caused by checker's exit i.e. SIGPIPE on writing to
	// stdout or read() on stdin returning 0 (thus producing UB in solutions
	// that assume the input is valid)
	(void)close(STDIN_FILENO);
	(void)close(STDOUT_FILENO);

	// Give some time for solution to hang on its stdout pipe's end before we exit
	using namespace std::chrono;
	auto now = system_clock::now();
	while (system_clock::now() - now < milliseconds(4)) {} // sleep is forbidden

	verdict.ok(42, "Partial points");
}

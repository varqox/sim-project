#include <array>
#include <cmath>
#include <csignal>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <type_traits>
#include <variant>
#include <vector>

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
	std::variant<OK, WRONG> wrong_override = WRONG{};

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

constexpr inline char space = ' ', newline = '\n';
struct EofType {
} eof;
struct IgnoreSpacesType {
} ignore_spaces;
struct IgnoreWsType {
} ignore_ws; // Every whitespace

constexpr inline auto newline_or_eof = optional(newline);

template <class T>
struct integer {
	T& val;
	T min_val, max_val;

	template <class T1, class T2>
	constexpr integer(T& val, T1 min_val, T2 max_val)
	: val([&]() -> T& {
		my_assert(numeric_limits<T>::min() <= min_val);
		my_assert(min_val <= max_val);
		my_assert(max_val <= numeric_limits<T>::max());
		return val;
	}())
	, min_val(min_val)
	, max_val(max_val) {}
};

template <size_t N>
struct character {
	char& val;
	array<char, N> options;

	template <class... Opts>
	constexpr explicit character(char& c, Opts... opts)
	: val(c)
	, options{opts...} {
		static_assert((std::is_same_v<Opts, char> and ...));
	}
};

template <class... Opts>
character(char&, Opts...) -> character<sizeof...(Opts)>;

class Scanner {
public:
	enum class Mode { STRICT, IGNORE_WS_BEFORE_EOF };

private:
	FILE* file_;
	Mode mode_;
	size_t line_ = 1;
	bool eofed = false;

	bool getchar(int& ch) noexcept {
		if (eofed) {
			return false;
		}

		ch = fgetc_unlocked(file_);
		eofed = (ch == EOF);
		line_ += (ch == '\n');
		return (not eofed);
	}

	void ungetchar(int ch) noexcept {
		ungetc(ch, file_);
		line_ -= (ch == '\n');
		eofed = false;
	}

	template <class... Args>
	void fatal(Args&&... message) {
		verdict.wrong("Line ", line_, ": ", std::forward<Args>(message)...);
	}

	static string char_description(int ch) {
		if (std::isgraph(ch)) {
			return {'\'', static_cast<char>(ch), '\''};
		}

		if (ch == ' ') {
			return "space";
		}
		if (ch == '\n') {
			return "'\\n'";
		}
		if (ch == '\r') {
			return "'\\r'";
		}
		if (ch == '\t') {
			return "'\\t'";
		}
		if (ch == '\0') {
			return "'\\0'";
		}

		constexpr char digits[] = "0123456789abcdef";
		return {'\'', '\\', 'x', digits[ch >> 4], digits[ch & 15], '\''};
	}

	Scanner& scan_char(char exp_ch) {
		int ch = 0;
		if (not getchar(ch)) {
			fatal("Read EOF, expected ", char_description(ch));
		}
		if (ch != exp_ch) {
			fatal("Read ", char_description(ch), ", expected ",
			      char_description(exp_ch));
		}

		return *this;
	}

	template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
	static constexpr bool is_digit(T ch) noexcept {
		return ('0' <= ch and ch <= '9');
	}

	template <class T>
	void scan_integer(T& val) {
		int ch = 0;
		if (not getchar(ch)) {
			fatal("Read EOF, expected a number");
		}

		bool minus = false;
		if (not std::is_unsigned_v<T> and ch == '-') {
			minus = true;
			if (not getchar(ch)) {
				fatal("Read EOF, expected a number");
			}
		}

		if (not is_digit(ch)) {
			fatal("Read ", char_description(ch), ", expected a number");
		}

		val = (minus ? '0' - ch : ch - '0'); // Will not overflow
		for (;;) {
			if (not getchar(ch)) {
				break;
			}

			if (not isdigit(ch)) {
				ungetchar(ch);
				break;
			}

			if (__builtin_mul_overflow(val, 10, &val)) {
				fatal("Integer value out of range");
			}

			if (not minus and __builtin_add_overflow(val, ch - '0', &val)) {
				fatal("Integer value out of range");
			}

			if (minus and __builtin_sub_overflow(val, ch - '0', &val)) {
				fatal("Integer value out of range");
			}
		}
	}

public:
	Scanner(FILE* file, Mode mode)
	: file_(file)
	, mode_(mode) {
		my_assert(file != nullptr);
	}

	Scanner(const Scanner&) = delete;
	Scanner(Scanner&&) = delete;
	Scanner& operator=(const Scanner&) = delete;
	Scanner& operator=(Scanner&&) = delete;

	~Scanner() {
		switch (mode_) {
		case Mode::STRICT: break;
		case Mode::IGNORE_WS_BEFORE_EOF: *this >> ignore_ws; break;
		}

		*this >> eof;
	}

	Scanner& operator>>(const char& exp_ch) {
		int ch = 0;
		if (not getchar(ch)) {
			fatal("Read EOF, expected ", char_description(ch));
		}
		if (ch != exp_ch) {
			fatal("Read ", char_description(ch), ", expected ",
			      char_description(exp_ch));
		}

		return *this;
	}

	Scanner& operator>>(const optional<char>& exp_ch) {
		int ch = 0;
		if (not getchar(ch)) {
			return *this;
		}

		if (exp_ch and ch != *exp_ch) {
			fatal("Read ", char_description(ch), ", expected EOF or ",
			      char_description(*exp_ch));
		}

		return *this;
	}

private:
	static string space_char_description(unsigned char ch) {
		return ' ' + char_description(ch);
	}

	template <size_t N, size_t... Idx>
	void read_character(character<N> c,
	                    std::index_sequence<Idx...> /*unused*/) {
		static_assert(N == sizeof...(Idx));
		int ch = 0;
		if (not getchar(ch)) {
			fatal("Read EOF, expected one of characters:",
			      space_char_description(c.options[Idx])...);
		}

		bool matches = ((ch == c.options[Idx]) or ...);
		if (not matches) {
			fatal("Read ", char_description(ch),
			      ", expected one of characters:",
			      space_char_description(c.options[Idx])...);
		}

		c.val = ch;
	}

public:
	template <size_t N>
	Scanner& operator>>(character<N> c) {
		read_character(c, std::make_index_sequence<N>());
		return *this;
	}

	Scanner& operator>>(char& c) {
		int ch = 0;
		if (not getchar(ch)) {
			fatal("Read EOF, expected a character");
		}

		c = static_cast<char>(ch);
		return *this;
	}

	Scanner& operator>>(EofType /*unused*/) {
		int ch = 0;
		if (getchar(ch)) {
			fatal("Read ", char_description(ch), ", expected EOF");
		}

		return *this;
	}

	Scanner& operator>>(IgnoreSpacesType /*unused*/) {
		int ch = 0;
		while (getchar(ch)) {
			if (ch != ' ') {
				ungetchar(ch);
				break;
			}
		}

		return *this;
	}

	Scanner& operator>>(IgnoreWsType /*unused*/) {
		int ch = 0;
		while (getchar(ch)) {
			if (not isspace(ch)) {
				ungetchar(ch);
				break;
			}
		}

		return *this;
	}

	// Does not omit white-spaces
	Scanner& operator>>(string& str) {
		int ch = 0;
		str.clear();
		while (getchar(ch)) {
			if (isspace(ch)) {
				ungetchar(ch);
				break;
			}

			str += static_cast<char>(ch);
		}

		return *this;
	}

	template <class T>
	Scanner& operator>>(integer<T>&& integer) {
		scan_integer(integer.val);
		if (integer.val < integer.min_val) {
			fatal("Integer ", integer.val, " is too small (smaller than ",
			      integer.min_val, ')');
		}
		if (integer.val > integer.max_val) {
			fatal("Integer ", integer.val, " is too big (bigger than ",
			      integer.max_val, ')');
		}

		return *this;
	}
};

int main(int argc, char** argv) {
	// argv[0] command (ignored)
	// argv[1] test_in
	// argv[2] test_out (right answer)
	// argv[3] answer to check
	//
	// Output (to stderr):
	// Line 1: "OK" or "WRONG"
	// Line 2 (optional; ignored if line 1 == "WRONG" - score is set to 0
	// anyway):
	//   leave empty or provide a real number x from interval [0, 100], which
	//   means that the test will get no more than x percent of its maximal
	//   score.
	// Line 3 and next (optional): A checker comment

	my_assert(argc == 4);

	ifstream in(argv[1]); // You can comment it out if you don't use it
	my_assert(in.is_open());
	ifstream out(argv[2]); // You can comment it out if you don't use it
	my_assert(out.is_open());
	Scanner scan(fopen(argv[3], "re"), Scanner::Mode::IGNORE_WS_BEFORE_EOF);

	int n;
	in >> n;
	int k;
	scan >> integer(k, 1, 1e9);
	if (k % n != 0) {
		verdict.wrong("Invalid modulo remainder");
	}
	verdict.ok();
}

// Krzysztof Małysa
#define _USE_MATH_DEFINES
#include <bits/stdc++.h>
using namespace std;

#define FOR(i,a,n) for (auto i ## __ = (n), i = (a); i <= i ## __; ++i)
#define FORD(i,a,n) for (auto i ## __ = (n), i = (a); i >= i ## __; --i)
#define REP(i,n) FOR(i,0, n - 1)
#define ALL(h) begin(h), end(h)
#define EB emplace_back
#define X first
#define Y second
#define V vector
#define tpv typedef V<

typedef long long LL;
typedef pair<int, int> PII;
tpv int> VI;
tpv VI> VVI;
tpv PII> VPII;
tpv LL> VLL;

constexpr char nl = '\n';
#define endl nl
#define ris return *this
#define tem template<class t

tem, class u> inline void mini(t& a, u&& b) { if (b < a) a = b; }
tem, class u> inline void maxi(t& a, u&& b) { if (b > a) a = b; }
int ceil2(int h) { return h < 2 ? 1 : 1 << (sizeof(h) * 8 - __builtin_clz(h - 1)); }

tem> struct Dump { t a, b; };
tem> auto dump(t&& h) -> Dump<decltype(begin(h))> { return {ALL(h)}; }
tem> auto stub(t* h) -> decltype(cerr << *h, 0);
tem> char stub(...);
#define enif(o) tem> typename enable_if<sizeof stub<t>(0) o 1, debug&>::type operator<<(t h)
#define dor > debug& operator<<
struct debug {
#ifdef DEBUG
#define deb debug()
	~debug() { cerr << nl; }
	enif(!=) { cerr << boolalpha << h; ris; }
	enif(==) {
		*this << '{';
		for (auto a = begin(h), b = end(h); a != b;)
			*this << *a++ << &" "[a == b];
		ris << '}';
	}
	tem, class u dor(pair<t, u> p) { ris << '(' << p.X << ", " << p.Y << ')'; }
	tem dor(Dump<t> d) {
		*this << "{\n";
		for (t a = d.a, c = a; a != d.b; ++a)
			*this << "  " << distance(c, a) << ": " << *a << nl;
		ris << '}';
	}
#else
	operator int() { return 0; }
	tem dor(t&&) { ris; }
#define deb 0 and debug()
#endif
};

#define imie(h...) #h ": " << (h) << " "
#define LOG(h...) deb << imie(h)
#define DOG(h...) deb << #h ": " << dump(h) << " "

mt19937_64 gen__(chrono::system_clock::now().time_since_epoch().count());

LL rd(LL a, LL b) { return uniform_int_distribution<LL>(a, b)(gen__); }

int main(int argc, char **argv) {
	ios::sync_with_stdio(false);
	cin.tie(nullptr);

	assert(argc - 1 == 4 && "Invalid number of arguments");
	// e.g.: ./gen 1 10 100 200
	int n = rd(atoi(argv[1]), atoi(argv[2]));
	long long min_val = atoll(argv[3]);
	long long max_val = atoll(argv[4]);

	VLL v(n);
	for (auto& x : v)
		x = rd(min_val, max_val);

	sort(ALL(v));

	cout << n << ' ' << v[rd(0, n - 1)] << nl;
	REP (i, n)
		cout << v[i] << " \n"[i + 1 == n];

	// system("cat /proc/$PPID/status | grep VmPeak >&2");
	return 0;
}

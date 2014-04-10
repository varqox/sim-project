#include <cstdio>
#include <vector>
#include <utility>
#include <functional>
#include <set>
#include <string>
#include <queue>

using namespace std;

#define FOR(x,b,e) for(int x = b; x < (e); ++x)
#define FOR2(x,b,e) for(int x = b; x <= (e); ++x)
#define FORD(x,b,e) for(int x = b; x >= (e); --x)
#define REP(x,n) for(int x = 0; x < (n); ++x)
#define REP2(x,n) for(int x = 0; x <= (n); ++x)
#define VAR(v,n) typeof(n) v = (n)
#define FOREACH(i,c) for(VAR(i, (c).begin()); i != (c).end(); ++i)
#define PB push_back
#define ST first
#define ND second
#define PF push_front
#define MP make_pair

typedef long long LL;
typedef unsigned long long ULL;
typedef vector<int> VI;
typedef vector<VI> VVI;
typedef vector<LL> VLL;
typedef vector<string> VS;
typedef pair<int, int> PII;
typedef vector<PII> VPII;
typedef vector<VPII> VVPII;

#define MOD(x) if(x>=1000000009) x%=1000000009

template<class T>
inline T abs(T x)
{return x<T() ? -x : x;}

#ifdef DEBUG
#include <iostream>

#define D(x) (x)
#define LOG(x) cerr << #x << ": " << x;
#define LOGN(x) cerr << #x << ": " << x << endl;

ostream& operator<<(ostream& os, const PII& myp)
{return os << "(" << myp.ST << ',' << myp.ND << ")";}

#else
#define D(x)
#define LOG(x)
#define LOGN(x)
#endif

#include <algorithm>

VLL fib(2);

int out(LL x)
{
	// static int lol=0;
	// ++lol;
	// if(lol>30) return 0;
	LL low=*--upper_bound(fib.begin(), --fib.end(), x), up=*upper_bound(fib.begin(), --fib.end(), x);
	LOG(x);
	D(cerr << ": ");
	LOG(low);
	D(cerr << ' ');
	LOGN(up);
	if(low==x)
		return 1;
	else if(x-low<=up-x)
		return 1+out(x-low);
	else
		return 1+out(up-x);
}

int main()
{
	fib[1]=1;
	FOR(i, 2, 87)
		fib.push_back(fib.back()+*++fib.rbegin());
#ifdef DEBUG
	REP(i, static_cast<int>(fib.size()))
		printf("%i: %lli\n", i, fib[i]);
#endif
	LL x;
	int p;
	scanf("%i", &p);
	REP(i, p)
	{
		scanf("%lli", &x);
		printf("%i\n", out(x));
	}
	return 0;
}

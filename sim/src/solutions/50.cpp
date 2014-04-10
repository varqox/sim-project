#include <cstdio>
#include <vector>
#include <utility>
#include <functional>
#include <set>
#include <string>
#include <cstring>
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

VI PalRad(const char *x, bool p)
{
	int n=strlen(x), i=1, j=0, k;
	VI r(n, 0);
	while(i<n)
	{
		while(i+p+j<n && i>j && x[i-j-1]==x[i+j+p])
			++j;
		for(r[i]=j, k=0; ++k<=j && r[i-k]!=j-k;)
			r[i+k]=min(r[i-k], j-k);
		j=max(0,j-k);
		i+=k;
	}
return r;
}

#include <iostream>

int main()
{
	int size, size2;
	string text;
	cin >> text;
	size=text.size();
	size2=size/2+(size&1);
	LOGN(size);
	LOGN(size2);
	text+=text;
	VI out=PalRad(text.c_str(), size&1);
	REP(i, int(out.size()))
	{
		LOG(i);
		D(cerr << ": " << out[i] << endl);
		if(out[i]>=size2)
		{
			printf("TAK\n");
			return 0;
		}
	}
	printf("NIE\n");
	return 0;
}
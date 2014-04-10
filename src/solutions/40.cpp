
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <vector>
#include <queue>
#include <map>
#include <cctype>
#include <cstring>
#include <algorithm>

using namespace std;

#define REP(i,x) for(int i=0; i<(x); ++i)
#define REP2(i,x) for(int i=0; i<=(x); ++i)
#define FOR(i,a,x) for(int i=(a); i<(x); ++i)
#define FOR2(i,a,x) for(int i=(a); i<=(x); ++i)
#define FORD(i,a,x) for(int i=(a); i>=(x); --i)
#define ALL(x) x.begin(), x.end()
#define VAR(a,x) typeof(x) a=(x)
#define FOREACH(i,x) for(VAR(i,x.begin()); i!=x.end(); ++i)
#define FOREACH2(i,x) for(VAR(i,x.rbegin()); i!=x.rend(); ++i)

#define I(x) static_cast<int>(x)
#define U(x) static_cast<unsigned>(x)

#define ST first
#define ND second
#define MP make_pair
#define PB push_back

typedef long long LL;
typedef vector<int> VI;
typedef vector<VI> VVI;
typedef vector<LL> VLL;
typedef vector<VLL> VVLL;
typedef pair<int, int> PII;
typedef vector<PII> VPII;
typedef vector<VPII> VVPII;

#ifdef DEBUG
#define D(x) x
#define LOG(x) cerr << #x << ": " << x
#define LOGN(x) cerr << #x << ": " << x << endl

ostream& operator<<(ostream& os, const PII& _)
{return os << "(" << _.ST << "," << _.ND << ")";}

template<class T>
ostream& operator<<(ostream& os, const vector<T>& _)
{
	if(_.empty())
		return os << "{}";
	os << "{" << *_.begin();
	for(typename vector<T>::const_iterator i=++_.begin(); i!=_.end(); ++i)
		os << " " << *i;
return os << "}";
}

template<class T>
ostream& operator<<(ostream& os, const deque<T>& _)
{
	if(_.empty())
		return os << "{}";
	os << "{" << *_.begin();
	for(typename deque<T>::const_iterator i=++_.begin(); i!=_.end(); ++i)
		os << " " << *i;
return os << "}";
}

template<class T>
ostream& operator<<(ostream& os, const set<T>& _)
{
	if(_.empty())
		return os << "{}";
	os << "{" << *_.begin();
	for(typename set<T>::const_iterator i=++_.begin(); i!=_.end(); ++i)
		os << " " << *i;
return os << "}";
}

#else
#define D(x)
#define LOG(x)
#define LOGN(x)
#endif

VVI col;

class cmp
{
public:
	bool operator()(const PII& a, const PII& b) const
	{return (a.ST==b.ST ? a.ND<b.ND : a.ST<b.ST);}
};

int main()
{
	int n, k, a, b;
	scanf("%i%i", &n, &k);
	VI arr(n);
	col.resize(k+1);
	REP(i, n)
	{
		scanf("%i", &arr[i]);
		col[arr[i]].PB(i);
	}
	scanf("%i%i", &a, &b);
	VI t1(a), t2(b);
	FOREACH(i, t1)
		scanf("%i", &*i);
	FOREACH(i, t2)
		scanf("%i", &*i);
	LOGN(n);
	LOGN(a);
	LOGN(b);
	LOGN(arr);
	LOGN(t1);
	LOGN(t2);
	LOGN(col);
	VVPII w2(b);
	FOREACH(it, col[*t2.rbegin()])
		w2[0].PB(MP(*it, -1));
	VAR($, t2.rbegin());
	REP(i, b-1)
	{
		++$;
		LOGN(*$);
		if(col[*$].empty())
		{
			printf("0\n");
			return 0;
		}
		VAR(x, col[*$].begin());
		w2[i+1].PB(MP(*x,-1));
		FOREACH(it, w2[i])
		{
			if(it->ST<*x) it->ND=w2[i+1].size()-1;
			else if(x!=col[*$].end())
			{
				while(x+1!=col[*$].end() && it->ST>*x)
					++x;
				if(it->ST<*x)
				{
					w2[i+1].PB(MP(*x, -1));
					it->ND=w2[i+1].size()-1;
				}
			}
		}
	}
	LOGN(w2);
	FOREACH(i, w2[b-1])
		i->ND=i->ST;
	LOGN(w2);
	FORD(i, b-2, 0)
	{
		FOREACH(it, w2[i])
		{
			if(it->ND==-1) break;
			if(w2[i+1][it->ND].ND==-1)
				it->ND=-1;
			else
				it->ND=w2[i+1][it->ND].ND;
		}
	}
	LOGN(w2);
	D(cerr << "--------------------------------------\n");
	VVPII w1(a);
	FOREACH2(it, col[*t1.rbegin()])
		w1[0].PB(MP(*it, -1));
	VAR($$, t1.rbegin());
	REP(i, a-1)
	{
		++$$;
		LOGN(*$$);
		if(col[*$$].empty())
		{
			printf("0\n");
			return 0;
		}
		VAR(x, col[*$$].rbegin());
		w1[i+1].PB(MP(*x,-1));
		FOREACH(it, w1[i])
		{
			if(it->ST>*x) it->ND=w1[i+1].size()-1;
			else if(x!=col[*$$].rend())
			{
				while(x+1!=col[*$$].rend() && it->ST<*x)
					++x;
				if(it->ST>*x)
				{
					w1[i+1].PB(MP(*x, -1));
					it->ND=w1[i+1].size()-1;
				}
			}
		}
	}
	LOGN(w1);
	FOREACH(i, w1[a-1])
		i->ND=i->ST;
	LOGN(w1);
	FORD(i, a-2, 0)
	{
		FOREACH(it, w1[i])
		{
			if(it->ND==-1) break;
			if(w1[i+1][it->ND].ND==-1)
				it->ND=-1;
			else
				it->ND=w1[i+1][it->ND].ND;
		}
	}
	LOGN(w1);
	VAR(i1, w2[0].begin());
	VAR(i2, w1[0].rbegin());
	VI out, fast;
	VPII yt;
	FOREACH(ii, col)
	{
		if(ii->size()>1)
			yt.PB(MP(ii->front(), ii->back()));
	}
	LOGN(yt);
	sort(yt.begin(), yt.end(), cmp());
	fast.resize(yt.size());
	if(yt.size())
		fast[0]=yt[0].ND;
	FOR(i, 1, yt.size())
		fast[i]=max(yt[i].ND, fast[i-1]);
	LOGN(yt);
	LOGN(fast);
	VPII::const_iterator ser;
	while(i1!=w2[0].end())
	{
		int beg=i2->ND, end=i1->ND;
		LOG(i1->ST)D(<<" -> [" << beg << " ; " << end << "]" << endl);
		if(beg>end) goto cont;
		/// Something...
		/*FOREACH(xx, yt)
			if(xx->ST<beg && xx->ND>end)
			{
				out.PB(i1->ST+1);
				goto cont;
			}*/
		ser=upper_bound(yt.begin(), yt.end()-1, MP(beg-1, 1000000000), cmp());
		LOGN(*ser);
		if(ser!=yt.begin() && !(ser->ST<beg && fast[ser-yt.begin()]>end))
			--ser;
		LOGN(*ser);
		LOGN(fast[ser-yt.begin()]);
		if(ser->ST<beg && fast[ser-yt.begin()]>end)
			out.PB(i1->ST+1);
	cont:
		++i1;
		++i2;
	}
	printf("%i\n", out.size());
	FOREACH(g, out)
		printf("%i ", *g);
	printf("\n");
return 0;
}

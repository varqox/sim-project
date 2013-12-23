// Zadanie: Bar sałatkowy XXI OI - etap 1
// Autor: Krzysztof Małysa
// Złożoność: czasowa - O(n lg n)

#include <algorithm>
#include <iostream>
#include <cstdio>
#include <vector>
#include <set>

using namespace std;

struct compare_first
{
	bool operator()(const pair<int, int>& a, const pair<int, int>& b) const
	{return a.first<b.first;}
};

struct compare_second
{
	bool operator()(const pair<int, int>& a, const pair<int, int>& b) const
	{return a.second<b.second;}
};

int main()
{
	int n, y, out=0;
	scanf("%i", &n);
	vector<int> f(2*n+2, -2);
	vector<pair<int, int> > front(n), back(n); // front ->, back <-
	vector<char> in(n);
	char c=getchar();
	f[y=n+1]=-1;
	for(int i=0; i<n; ++i)
	{
		back[i].second=i;
		in[i]=c=getchar();
		if(c=='p')
		{
			--y;
			back[i].first=f[y-1]+2;
		}
		else
		{
			++y;
			back[i].first=n;
		}
		f[y]=i;
	}
	for(int i=0; i<=n; ++i)
		f[i]=n+1;
	f[y=n+1]=n;
	for(int i=n-1; i>=0; --i)
	{
		front[i].first=i;
		if(in[i]=='p')
		{
			--y;
			front[i].second=f[y-1]-2;
		}
		else
		{
			++y;
			front[i].second=-1;
		}
		f[y]=i;
	}
	sort(back.begin(), back.end(), compare_first());
	set<pair<int, int>, compare_second> my_set; // ends ale different
	for(int i=0, j=0; i<n; ++i)
	{
		if(front[i].first>front[i].second) continue;
		while(back[j].first<=front[i].first)
		{
			if(back[j].first<=back[j].second)
				my_set.insert(back[j]);
			++j;
		}
		set<pair<int, int>, compare_second>::iterator k=my_set.upper_bound(front[i]);
		--k;
		out=max(out, k->second-front[i].first+1);
	}
	printf("%i\n", out);
return 0;
}
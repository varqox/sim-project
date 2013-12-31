// Zadanie: Hotele XXI OI - etap 1
// Autor: Krzysztof Małysa
// Złożoność: czasowa - O(n^2), pamięciowa - O(n)

#include <cstdio>
#include <vector>
#include <queue>

using namespace std;

int n;
vector<int> *G;

long long get(unsigned centr)
{
	vector<char> col(n+1, -1);
	int deep=1, qsize, st_centr=G[centr].size();
	long long K1, K2, K3, ret=static_cast<long long>(st_centr-2)*(st_centr-1)*st_centr/6;
	if(st_centr<3) return 0;
	vector<int> out(st_centr);
	queue<pair<int, int> > q;
	col[centr]=0;
	for(int vvv=0; vvv<st_centr; ++vvv)
	{
		q.push(make_pair(G[centr][vvv], vvv));
		col[G[centr][vvv]]=0;
	}
	deep=1;
	qsize=st_centr;
	while(!q.empty())
	{
		if(qsize==0)
		{
			K1=K2=K3=0;
			register long long tmp;
			for(int j=0; j<st_centr; ++j)
			{
				tmp=out[j];
				out[j]=0;
				K1+=tmp;
				K2+=tmp*tmp;
				K3+=tmp*tmp*tmp;
			}
			ret+=(K1*(K1*K1-3*K2)+2*K3)/6;
			++deep;
			qsize=q.size();
		}
		for(int i=0; i<G[q.front().first].size(); ++i)
		{
			if(col[G[q.front().first][i]]==-1)
			{
				++out[q.front().second];
				col[G[q.front().first][i]]=deep;
				q.push(make_pair(G[q.front().first][i], q.front().second));
			}
		}
		q.pop();
		--qsize;
	}
return ret;
}

int main()
{
	int a, b;
	scanf("%i", &n);
	G=new vector<int>[n+1];
	for(int i=1; i<n; ++i)
	{
		scanf("%i %i", &a, &b);
		G[a].push_back(b);
		G[b].push_back(a);
	}
	long long out=0;
	for(int i=1; i<=n; ++i)
		out+=get(i);
	printf("%lli\n", out);
return 0;
}
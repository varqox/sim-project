#include <iostream>
#include <vector>

using namespace std;

struct par
{
	long long int a, b;
	par(int a1=0, int b1=0): a(a1), b(b1){}
};

int main()
{
	ios_base::sync_with_stdio(false);
	int n;
	cin >> n;
	vector<char> arr(n);
	vector<par> out;
	for(int i=0; i<n; ++i)
	{
		cin >> arr[i];
		arr[i]=(arr[i]=='p' ? 1:-1);
	}
	int output=0;
	for(int i=0; i<n; ++i)
	{
		out.push_back(par());
		for(int j=0; j<=i; ++j)
		{
			out[j].a+=arr[i];
			if(out[j].a<0) out[j].a-=1<<20;
			if(arr[i]==1)
			{
				if(++out[j].b>=0 && out[j].a>=0) output=max(output, i-j+1);
			}
			else out[j].b=(out[j].b>0 ? -1:out[j].b-1);
		}
	}
	cout << output << endl;
return 0;
}
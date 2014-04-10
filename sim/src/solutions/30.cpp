#include <cstdio>
#include <algorithm>
using namespace std;
long long int fib[87],a,ile,licznik;



long long int najm(long long int a)
{
	long long int liczba=1;
	long long int roznica=a-1;
	for(int i=2;i<87;++i)
	{
		if(roznica>abs(a-fib[i]))
		{
			roznica=abs(a-fib[i]);
			liczba=fib[i];
		}
		else break;
	}
	return liczba;
}

int main()
{
	fib[0]=1;fib[1]=1;
	for(int i=2;i<87;++i)fib[i]=fib[i-1]+fib[i-2];
	scanf("%lld",&ile);
	for(int i=0;i<ile;++i)
	{
		scanf("%lld",&a);
		while(a!=0)
		{
			a=abs(a-najm(a));
			licznik++;
		}
		printf("%lld\n",licznik);
		licznik=0;
	}
	
	return 0;
}

#include<cstdio>
#include<deque>

using namespace std;

int tops[1000001];
int left[1000001];
int right[1000001];
int size,stosunek,licznik;
char s[1000001];
deque<char> tab;

bool malysa_jest_glupi=true; // :)

int main(){
	scanf("%d",&size);
	scanf("%s",s);
	if(malysa_jest_glupi){
		for(int i=0; i<size; ++i){
			stosunek=0,licznik=0;
			for(int j=i; j>=0; --j){
				if(s[j]=='p')++stosunek;
				else --stosunek;
				if(stosunek<0)break;
				++licznik;
			}
			left[i]=licznik;
			stosunek=0,licznik=0;
			for(int j=i; j<size; ++j){
				if(s[j]=='p')++stosunek;
				else --stosunek;
				if(stosunek<0)break;
				++licznik;
			}
			right[i]=licznik;
		}
		for(int i=size-1; i>=0; --i){
			for(int j=0; j<size-i; ++j){
				if(s[j]=='p' && s[i+j]=='p'){
					if(right[j]>=(i+1) && left[i+j]>=(i+1)){
						printf("%d\n",(i+1));
						return 0;
					}
				}
			}
		}
		printf("0\n");
	}
	return 0;
}

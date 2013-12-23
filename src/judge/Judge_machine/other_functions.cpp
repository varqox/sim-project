#include <fstream>
#include <string>
#include <deque>

using namespace std;

string myto_string(long long int a)
{
	string w;
	while(a>0)
	{
		w=static_cast<char>(a%10+'0')+w;
		a/=10;
	}
	if(w.empty()) w="0";
return w;
}

string make_safe_php_string(const string& str)
{
	string out;
	for(int i=0; i<str.size(); ++i)
	{
		if(str[i]=='\'') out+="\\'";
		else if(str[i]=='\\') out+="\\\\";
		else out+=str[i];
	}
return out;
}

string make_safe_html_string(const string& str)
{
	string out;
	for(int i=0; i<str.size(); ++i)
	{
		if(str[i]=='<') out+="&lt;";
		if(str[i]=='>') out+="&gt;";
		if(str[i]=='&') out+="&amp;";
		else out+=str[i];
	}
return out;
}

deque<int> kmp(const string& text, const string& pattern)
{
	deque<int> out;
	int *P=new int[pattern.size()], k=0, pl=pattern.size();
	P[0]=0;
	for(int i=1; i<pl; ++i)
	{
		while(k>0 && pattern[k]!=pattern[i])
			k=P[k-1];
		if(pattern[k]==pattern[i]) ++k;
		P[i]=k;
	}
	k=0;
	for(int tl=text.size(), i=0; i<tl; ++i)
	{
		while(k>0 && pattern[k]!=text[i])
			k=P[k-1];
		if(pattern[k]==text[i]) ++k;
		if(k==pl)
		{
			out.push_back(i);
			k=P[k-1];
		}
	}
	delete[] P;
return out;
}

string file_get_contents(const string& file_name)
{
	string out, tmp;
	fstream file(file_name.c_str(), ios_base::in);
	if(file.good())
	{
		getline(file, out);
		while(file.good())
		{
			getline(file, tmp);
			out+='\n';
			out+=tmp;
		}
	}
return out;
}
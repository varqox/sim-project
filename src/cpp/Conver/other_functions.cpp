#include <fstream>
#include <string>
#include <deque>
#include <cstdlib>
#include <dirent.h>
#include <cstring>

using namespace std;

void remove_r(const char* path)
{
	DIR* directory;
	dirent* current_file;
	string tmp_dir_path=path;
	if(*tmp_dir_path.rbegin()!='/') tmp_dir_path+='/';
	if((directory=opendir(path))) {
		while((current_file=readdir(directory)))
			if(strcmp(current_file->d_name, ".") && strcmp(current_file->d_name, ".."))
				remove_r((tmp_dir_path+current_file->d_name).c_str());
		closedir(directory);
	}
	remove(path);
}

string myto_string(long long a)
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
	for(unsigned i=0; i<str.size(); ++i)
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
	for(unsigned i=0; i<str.size(); ++i)
	{
		if(str[i]=='<') out+="&lt;";
		if(str[i]=='>') out+="&gt;";
		if(str[i]=='&') out+="&amp;";
		else out+=str[i];
	}
return out;
}

deque<unsigned> kmp(const string& text, const string& pattern)
{
	deque<unsigned> out;
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

int comparePrefix(const string& str, const string& prefix) {
	for (size_t i = 0, len = min(str.size(), prefix.size()); i < len; ++i) {
		if (str[i] < prefix[i])
			return -1;
		if (str[i] > prefix[i])
			return 1;
	}
	return 0;
}

string tolower(string str) {
	for (size_t i = 0, s = str.size(); i < s; ++i)
		str[i] = tolower(str[i]);
	return str;
}

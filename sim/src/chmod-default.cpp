#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstdio>

using namespace std;

string without_end_after_slash(const string& str)
{
	int i=str.size();
	while(--i>=0 && str[i]!='/');
	if(i>=0 && str[i]=='/') return string(str, 0, i+1);
return str;
}

void change(string name)
{
	char* tmp_file_name=tmpnam(NULL);
	system(("chmod 0755 "+name+" && ls -p -1 "+name+string(" > ")+tmp_file_name).c_str());
	cout << name << endl;
	fstream tmp_file(tmp_file_name, ios_base::in);
	if(tmp_file.good())
	{
		string new_name;
		while(!tmp_file.eof())
		{
			getline(tmp_file, new_name);
			if(new_name.empty()) continue;
			if(*--new_name.end()=='/')
			{
				system(("chmod 0755 "+name+new_name).c_str());
				change(name+new_name);
			}
			else
				system(("chmod 0644 "+name+new_name).c_str());
			cout << name+" : "+new_name+";" << endl;
		}
		tmp_file.close();
		remove(tmp_file_name);
	}
}

int main(int argc, char** argv)
{
	ios_base::sync_with_stdio(false);
	if(argc<2) cout << "Usage: <files...>" << endl;
	for(int i=1; i<argc; ++i)
		change(argv[i]);
return 0;
}
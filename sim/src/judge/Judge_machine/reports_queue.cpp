#include <algorithm>
#include <dirent.h>
#include <string>
#include <vector>
#include <cstdio>

using namespace std;

namespace reports_queue
{
// private
	const char* const QUEUE_DIR="queue/";
	vector<string> reports;

	void search_new();

// public
	bool empty();
	void pop();
	string extract();
	const string& front();
}
#include <iostream>
namespace reports_queue
{
	struct compare
	{
		bool operator()(const string& s1, const string& s2) const
		{return (s1.size()==s2.size() ? s1>s2:s1.size()>s2.size());}
	};

	void search_new()
	{
		reports.clear();
		DIR* directory;
		dirent* current_file;
		int	name_lenght;
		if((directory=opendir(QUEUE_DIR)))
			while((current_file=readdir(directory)))
				if(*current_file->d_name!='.')
					reports.push_back(current_file->d_name);
		sort(reports.begin(), reports.end(), compare());
		cerr << ' ' << reports.size() << ":\n";
		for(vector<string>::reverse_iterator i=reports.rbegin(); i!=reports.rend(); ++i)
			cerr << *i << endl;
	}

	bool empty()
	{
		if(reports.empty())
			search_new();
	return reports.empty();
	}

	void pop()
	{
		remove((QUEUE_DIR+reports.back()).c_str());
		reports.pop_back();
	}

	string extract()
	{
		if(!empty())
		{
			string out=reports.back();
			pop();
			return out;
		}
	return "";
	}

	const string& front()
	{return reports.back();}
}
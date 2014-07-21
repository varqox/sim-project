#include <algorithm>
#include <dirent.h>
#include <string>
#include <vector>
#include <cstdio>

#include "db.hpp"

// public
#include "reports_queue.hpp"

#ifdef DEBUG
#include <iostream>
#define D(x) x
#else
#define D(x)
#endif

using namespace std;

D(
std::ostream& operator<<(std::ostream& os, const reports_queue::report& r)
{return os << "(" << r.id() << ", " << r.task_id() << ")";}
)

string myto_string(long long int a);

namespace reports_queue
{
	void report::set(report_status st, long long points) const
	{
		sql::Statement *stmt = DB::mysql()->createStatement();
		stmt->execute("UPDATE reports SET status='"+to_str(st)+"',points="+myto_string(points)+" WHERE id="+_id);
		delete stmt;
	}

// private
	vector<report> reports;
	void search_new();
// --------------------------
	struct compare
	{
		bool operator()(const report& a, const report& b) const
		{return (a.id().size()==b.id().size() ? a.id()>b.id():a.id().size()>b.id().size());}
	};

	void search_new()
	{
		reports.clear();

		sql::Statement *stmt = DB::mysql()->createStatement();
		sql::ResultSet *res;
		res = stmt->executeQuery("SELECT id,task_id FROM reports WHERE status='waiting' LIMIT 30");
		while(res->next())
			reports.push_back(report(res->getString(1), res->getString(2)));
		delete res;
		delete stmt;
		sort(reports.begin(), reports.end(), compare());
		D(
		cerr << ' ' << reports.size() << ":\n";
		for(vector<report>::reverse_iterator i=reports.rbegin(); i!=reports.rend(); ++i)
			cerr << *i << endl;
		)
	}

	bool empty()
	{
		if(reports.empty())
			search_new();
	return reports.empty();
	}

	void pop()
	{reports.pop_back();}

	report extract()
	{
		if(!empty())
		{
			report out=reports.back();
			reports.pop_back();
			return out;
		}
	return report("", "");
	}

	const report& front()
	{return reports.back();}
}
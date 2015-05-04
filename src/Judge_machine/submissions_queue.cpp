#include <algorithm>
#include <dirent.h>
#include <string>
#include <vector>
#include <cstdio>

#include "db.hpp"

// public
#include "submissions_queue.h"

#ifdef DEBUG
#include <iostream>
#define D(x) x
#else
#define D(x)
#endif

using namespace std;

D(
std::ostream& operator<<(std::ostream& os, const submissions_queue::submission& r)
{return os << "(" << r.id() << ", " << r.problem_id() << ")";}
)

string toString(long long int a);

namespace submissions_queue
{
	void submission::set(submission_status st, long long points) const
	{
		sql::Statement *stmt = DB::mysql()->createStatement();
		stmt->execute("UPDATE submissions SET status='"+to_str(st)+"',points="+toString(points)+" WHERE id="+_id);
		delete stmt;
	}

// private
	vector<submission> submissions;
	void search_new();
// --------------------------
	struct compare
	{
		bool operator()(const submission& a, const submission& b) const
		{return (a.id().size()==b.id().size() ? a.id()>b.id():a.id().size()>b.id().size());}
	};

	void search_new()
	{
		submissions.clear();

		sql::Statement *stmt = DB::mysql()->createStatement();
		sql::ResultSet *res;
		res = stmt->executeQuery("SELECT id,problem_id FROM submissions WHERE status='waiting' ORDER BY queued LIMIT 30");
		while(res->next())
			submissions.push_back(submission(res->getString(1), res->getString(2)));
		delete res;
		delete stmt;
		sort(submissions.begin(), submissions.end(), compare());
		D(
		cerr << ' ' << submissions.size() << ":\n";
		for(vector<submission>::reverse_iterator i=submissions.rbegin(); i!=submissions.rend(); ++i)
			cerr << *i << endl;
		)
	}

	bool empty()
	{
		if(submissions.empty())
			search_new();
	return submissions.empty();
	}

	void pop()
	{submissions.pop_back();}

	submission extract()
	{
		if(!empty())
		{
			submission out=submissions.back();
			submissions.pop_back();
			return out;
		}
	return submission("", "");
	}

	const submission& front()
	{return submissions.back();}
}

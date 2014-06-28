#include <fstream>
#include <deque>

using namespace std;

void remove_trailing_spaces(string& str)
{
	string::iterator erase_begin=str.end();
	while(erase_begin!=str.begin() && isspace(*(erase_begin-1))) --erase_begin;
	str.erase(erase_begin, str.end());
}

int main(int argc, char **argv) // argv[0] command, argv[1] test_in, argv[2] test_out (right answer), argv[3] answer to check
{
	ios_base::sync_with_stdio(false);
	fstream out(argv[3], ios_base::in), ans(argv[2], ios_base::in);
	if(!out.good() && !ans.good())
		return 2;// Evaluation failure
	deque<string> out_in, ans_in;
	string out_tmp, ans_tmp;
	while(out.good() && ans.good())
	{
		getline(out, out_tmp);
		getline(ans, ans_tmp);
		remove_trailing_spaces(out_tmp);
		remove_trailing_spaces(ans_tmp);
		out_in.push_back(out_tmp);
		ans_in.push_back(ans_tmp);
	}
	while(!out_in.empty() && out_in.back().empty()) out_in.pop_back();
	while(!ans_in.empty() && ans_in.back().empty()) ans_in.pop_back();
	int line=-1;
	while(++line<out_in.size() && line<ans_in.size())
		if(ans_in[line]!=out_in[line])
			return 1; // Wrong answer
	if(ans_in.size()>out_in.size())
		return 1; // Wrong answer
return 0; // OK
}
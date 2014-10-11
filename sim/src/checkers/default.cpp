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
	fstream out(argv[2], ios_base::in), ans(argv[3], ios_base::in);
	if(!ans.good() && !out.good())
		return 2;// Evaluation failure
	deque<string> ans_in, out_in;
	string ans_tmp, out_tmp;
	while(ans.good() && out.good())
	{
		getline(ans, ans_tmp);
		getline(out, out_tmp);
		remove_trailing_spaces(ans_tmp);
		remove_trailing_spaces(out_tmp);
		ans_in.push_back(ans_tmp);
		out_in.push_back(out_tmp);
	}
	while(!ans_in.empty() && ans_in.back().empty()) ans_in.pop_back();
	while(!out_in.empty() && out_in.back().empty()) out_in.pop_back();
	int line=-1;
	while(++line<ans_in.size() && line<out_in.size())
		if(out_in[line]!=ans_in[line])
			return 1; // Wrong answer
	if(out_in.size()>ans_in.size())
		return 1; // Wrong answer
return 0; // OK
}
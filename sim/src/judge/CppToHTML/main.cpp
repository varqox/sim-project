#include "main.hpp"

const bool is_name[256]={false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, false, false, false, false, true, false, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false},
	is_true_name[256]={false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, true,true, true, true, true, true, true, true, true, true, false, false, false, false, false, false, false, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, false, false, false, false, true, false, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false};

int main(int argc, char const **argv)
{
	ios_base::sync_with_stdio(false);
	if(argc<2)
	{
		cerr << "Usage:\ncr <file name>" << endl;
		return 1;
	}
	string file_name=argv[1];
	// cin >> file_name;
	fstream file(file_name.c_str(), ios::in);
	if(file.good())
	{
		coloring::init();
		// fstream out((file_name+".html").c_str(), ios::out);
		cout << "<table style=\"background: #f5f5f5;border-spacing: 0;\ndisplay: inline-block;\nfont-size: 15px;\nfont-family: 'DejaVu Sans Mono';\nline-height: 18px;\nborder: 1px solid #afafaf;\nborder-radius: 4px;\">\n<tbody>\n<tr>\n<td style=\"padding: 0\">\n<pre style=\"color: #4c4c4c;\nmargin: 0;\ntext-align: right;\npadding: 5px 5px 5px 7px;border-right: 1px solid #afafaf\">\n1\n";
		string input, tmp;
		getline(file, input);
		unsigned line=1;
		while(file.good())
		{
			cout << ++line << '\n';
			input+='\n';
			getline(file, tmp);
			input+=tmp;
		}
		cout << "</pre>\n</td>\n<td style=\"padding: 0\">\n<pre style=\"text-align: left;tab-size: 4;margin: 0;padding: 5px 5px 5px 1em\">\n";
		coloring::color_code(input, cout);
		cout << "</pre></td></tr></tbody></table>";
	}
	else cout << "Cannot open file" << endl;
return 0;
}
#include <cstdlib>
#include <cstdio>
#include "main.hpp"

using namespace std;

compile compile::run;

bool compile::operator()(const string& report_id, const string& exec)
{
#ifdef SHOW_LOGS
	fprintf(stderr, "Compilation: %s to: %s\n", report_id.c_str(), exec.c_str());
#endif
	compile_errors="";
	int ret=system(("(cd ../solutions && timeout 15 g++ "+report_id+".cpp -s -O2 -static -lm -m32 -o ../judge/"+exec+") 2> "+string(file_compile_errors)).c_str());
	if(!ret)
	{
		remove(file_compile_errors.c_str());
		return true;
	}
	compile_errors=file_get_contents(file_compile_errors);
	if(compile_errors.empty()) compile_errors="Compile time limit";
	remove(file_compile_errors.c_str());
return false;
}
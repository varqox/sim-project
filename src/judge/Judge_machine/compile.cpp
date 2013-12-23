#include <cstdlib>
#include <cstdio>
#include "main.hpp"

using namespace std;

namespace compile
{
	string compile_errors;

	// Report ID, exec_name in chroot/
	bool run(const string& report_id, const string& exec)
	{
		fprintf(stderr, "Compilation: %s to: %s\n", report_id.c_str(), exec.c_str());
		compile_errors="";
		int ret=system(("(cd ../solutions && timeout 15 g++ "+report_id+".cpp -s -O2 -static -lm -m32 -o ../judge/chroot/"+exec+") 2> errors.log").c_str());
		if(!ret)
		{
			remove("errors.log");
			return true;
		}
		compile_errors=file_get_contents("errors.log");
		if(compile_errors.empty()) compile_errors="Compile time limit";
		remove("errors.log");
	return false;
	}
}
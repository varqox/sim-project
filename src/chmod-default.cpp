#include <string>
#include <cstdio>
#include <sys/stat.h> // chmod()
#include <dirent.h>
#include <cstring>

using namespace std;

//         file -> 0644     dir -> 0755
const int file_mod = 420, dir_mod = 493;

void def_chmod_r(const char* path, bool show_info = false)
{
	DIR* directory;
	dirent* current_file;
	string tmp_dir_path = path;
	if(*tmp_dir_path.rbegin() != '/') tmp_dir_path += '/';
	if((directory = opendir(path)))
	{
		while((current_file = readdir(directory)))
			if(strcmp(current_file->d_name, ".") && strcmp(current_file->d_name, ".."))
					def_chmod_r((tmp_dir_path+current_file->d_name).c_str(), show_info);
		if(show_info)
			printf("chmod 0755 -> %s\n", path);
		chmod(path, dir_mod);
	}
	else
	{
		if(show_info)
			printf("chmod 0644 -> %s\n", path);
		chmod(path, file_mod);
	}
}

int main(int argc, char const **argv)
{
	if(argc < 2)
		printf("Usage: chmod-default [option] <files...>\nOptions:\n  --show-info - enable printing info about changed files\n  --no-show-info - disable --show-info\n");
	bool show_info = false;
	for(int i = 1; i < argc; ++i)
	{
		if(0 == strcmp(argv[i], "--show-info"))
			show_info = true;
		else if(0 == strcmp(argv[i], "--no-show-info"))
			show_info = true;
		else
			def_chmod_r(argv[i], show_info);
	}
return 0;
}
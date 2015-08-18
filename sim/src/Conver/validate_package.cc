#include "validate_package.h"

#include "../simlib/include/debug.h"

#include <cerrno>
#include <cstring>
#include <dirent.h>

using std::string;

string validatePackage(string pathname) {
	DIR *dir = opendir(pathname.c_str()), *subdir;
	dirent *file;

	if (dir == NULL) {
		eprintf("Error: Failed to open directory: '%s' - %s\n",
			pathname.c_str(), strerror(errno));
		abort(); // Should not happen
	}

	if (*--pathname.end() != '/')
		pathname += '/';

	string main_folder;
	while ((file = readdir(dir))) {
		if (0 != strcmp(file->d_name, ".") && 0 != strcmp(file->d_name, ".."))
			if ((subdir = opendir((pathname + file->d_name).c_str()))) {
				if (main_folder.size()) {
					eprintf("Error: More than one package main folder found\n");
					exit(6);
				}
				closedir(subdir);
				main_folder = file->d_name;
			}
	}

	if (main_folder.empty()) {
		eprintf("Error: No main folder found in package\n");
		exit(6);
	}

	pathname.append(main_folder);
	if (*--pathname.end() != '/')
		pathname +='/';

	// Get package file structure
	package_tree_root.reset(directory_tree::dumpDirectoryTree(pathname));
	if (package_tree_root.isNull()) {
		eprintf("Failed to get package file structure: %s\n", strerror(errno));
		abort(); // This is probably a bug
	}

	// Validate conf.cfg
	if (!USE_CONF || !std::binary_search(package_tree_root->files.begin(),
			package_tree_root->files.end(), "conf.cfg") ||
			conf_cfg.loadConfig(pathname, (VERBOSITY >> 1) + 1) != 0)
		USE_CONF = false;

	return pathname;
}

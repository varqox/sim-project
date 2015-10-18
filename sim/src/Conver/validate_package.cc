#include "validate_package.h"

#include "../simlib/include/debug.h"

#include <dirent.h>

using std::string;

string validatePackage(string pathname) {
	DIR *dir = opendir(pathname.c_str()), *subdir;
	dirent *file;

	if (dir == nullptr) {
		eprintf("Error: Failed to open directory: '%s' - %s\n",
			pathname.c_str(), strerror(errno));
		abort(); // Should not happen
	}

	if (pathname.back() != '/')
		pathname += '/';

	// Check whether package has exactly one main folder
	string main_folder;
	while ((file = readdir(dir))) {
		if (0 != strcmp(file->d_name, ".") && 0 != strcmp(file->d_name, "..")
			&& (subdir = opendir((pathname + file->d_name).c_str()))) {
				if (main_folder.size()) {
					eprintf("Error: More than one package main folder found\n");
					exit(6);
				}

				closedir(subdir);
				main_folder = file->d_name;
			}
	}
	closedir(dir);

	if (main_folder.empty()) {
		eprintf("Error: No main folder found in package\n");
		exit(6);
	}

	pathname.append(main_folder);
	if (pathname.back() != '/')
		pathname +='/';

	// Get package file structure
	package_tree_root.reset(directory_tree::dumpDirectoryTree(pathname));
	if (package_tree_root.isNull()) {
		eprintf("Failed to get package file structure: %s\n", strerror(errno));
		abort(); // This is probably a bug
	}

	// Validate config.conf
	if (!USE_CONFIG || !std::binary_search(package_tree_root->files.begin(),
			package_tree_root->files.end(), "config.conf"))
		USE_CONFIG = false;
	else try {
		if (VERBOSITY > 1)
			printf("Validating config.conf...\n");
		config_conf.loadConfig(pathname);
		if (VERBOSITY > 1)
			printf("Validation passed.\n");

	} catch (std::exception& e) {
		USE_CONFIG = false;
		if (VERBOSITY > 0)
			eprintf("Error: %s\n", e.what());
	}

	return pathname;
}

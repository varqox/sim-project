#include "../../include/sim/problem_package.h"
#include "../../include/zip.h"

using std::string;

namespace sim {

void PackageContents::load_from_directory(string pkg_path) {
	throw_assert(pkg_path.size() > 0);
	if (pkg_path.back() != '/')
		pkg_path += '/';

	InplaceBuff<PATH_MAX> path;
	auto recursive_impl = [&](auto&& self, StringView filename) -> void {
		auto add_path_to_pc = [&] {
			// Adds entry without the pkg_path prefix
			this->add_entry(StringView(path.data() + pkg_path.size(),
				path.size - pkg_path.size())); // this-> is needed for buggy GCC
		};

		path.append(filename, '/'); // Update path
		add_path_to_pc();
		// Go through the directory entries
		forEachDirComponent(path.to_cstr(),
			[&](dirent* file) {
				if (file->d_type == DT_DIR)
					self(self, file->d_name);
				else {
					auto x = path.size;
					path.append(file->d_name);
					add_path_to_pc();
					path.size = x;
				}
			});

		path.size -= filename.size() + 1; // Remove "filename/" from the path
	};

	// Pass pkg_path without trailing '/' as this argument should not
	// have it
	recursive_impl(recursive_impl,
		StringView(pkg_path.data(), pkg_path.size() - 1));
}

void PackageContents::load_from_zip(CStringView pkg_path) {
	skim_zip(pkg_path, [&](archive_entry* entry) {
		// Check if entry contains ".." component
		StringView epath = archive_entry_pathname(entry);
		if (hasPrefix(epath, "../") or
			epath.find("/../") != StringView::npos)
		{
			THROW("Found invalid component \"../\" - archive is not safe"
				" for the further processing as it may be disambiguating");
		}

		add_entry(epath);
	});
}

template<class T>
static string zip_package_master_dir_impl(T&& pkg) {
	string res;
	skim_zip(pkg, [&](archive_entry* entry) {
		// Check if entry contains ".." component
		StringView epath = archive_entry_pathname(entry);
		if (hasPrefix(epath, "../") or
			epath.find("/../") != StringView::npos)
		{
			THROW("Found invalid component \"../\" - archive is not safe"
				" for the further processing as it may be disambiguating");
		}

		if (res.empty()) { // Init res
			auto pos = epath.find('/');
			if (pos == StringView::npos) {
				res = "";
				return false; // Break lookup
			} else
				res = epath.substr(0, pos + 1).to_string();

		} else if (not hasPrefix(epath, res)) {
			res = "";
			return false; // Break lookup
		}

		return true; // Everything is ok
	});

	return res;
}

string zip_package_master_dir(CStringView pkg_path) {
	return zip_package_master_dir_impl(pkg_path);
}

string zip_package_master_dir(int pkg_fd) {
	return zip_package_master_dir_impl(pkg_fd);
}

} // namespace sim

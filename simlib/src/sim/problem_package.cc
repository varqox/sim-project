#include "../../include/sim/problem_package.h"
#include "../../include/libarchive_zip.h"

using std::string;

namespace sim {

void PackageContents::load_from_directory(StringView pkg_path,
	bool retain_pkg_path_prefix)
{
	throw_assert(pkg_path.size() > 0);
	pkg_path.removeTrailing('/');

	InplaceBuff<PATH_MAX> path;
	auto recursive_impl = [&](auto&& self, StringView filename) -> void {
		auto add_path_to_pc = [&] {
			if (retain_pkg_path_prefix)
				this->add_entry(path); // this-> is needed for buggy GCC
			else // Adds entry without the pkg_path prefix
				this->add_entry(StringView(path).substr(pkg_path.size() + 1));
		};

		path.append(filename, '/'); // Update path
		if (path.size > filename.size() + 1)
			add_path_to_pc(); // do not add pkg_path as an entry

		// Go through the directory entries
		forEachDirComponent(path,
			[&](dirent* file) {
				if (file->d_type == DT_DIR) {
					self(self, file->d_name);
				} else {
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
	recursive_impl(recursive_impl, pkg_path);
}

void PackageContents::load_from_zip(FilePath pkg_path) {
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

string zip_package_master_dir(FilePath pkg_path) {
	return zip_package_master_dir_impl(pkg_path);
}

string zip_package_master_dir(int pkg_fd) {
	return zip_package_master_dir_impl(pkg_fd);
}

} // namespace sim

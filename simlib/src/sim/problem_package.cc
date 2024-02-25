#include <simlib/directory.hh>
#include <simlib/libzip.hh>
#include <simlib/sim/problem_package.hh>
#include <simlib/throw_assert.hh>

using std::string;

namespace sim {

void PackageContents::load_from_directory(StringView pkg_path, bool retain_pkg_path_prefix) {
    throw_assert(!pkg_path.empty());
    pkg_path.remove_trailing('/');

    InplaceBuff<PATH_MAX> path;
    auto recursive_impl = [&](auto&& self, StringView filename) -> void {
        auto add_path_to_pc = [&] {
            if (retain_pkg_path_prefix) {
                this->add_entry(path); // this-> is needed for buggy GCC
            } else { // Adds entry without the pkg_path prefix
                this->add_entry(StringView{path}.substr(pkg_path.size() + 1));
            }
        };

        path.append(filename, '/'); // Update path
        if (path.size > filename.size() + 1) {
            add_path_to_pc(); // do not add pkg_path as an entry
        }

        // Go through the directory entries
        for_each_dir_component(path, [&](dirent* file) {
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
    ZipFile zip(pkg_path, ZIP_RDONLY);
    auto size = zip.entries_no();
    for (zip_int64_t i = 0; i < size; ++i) {
        // Check if entry contains ".." component
        StringView epath = zip.get_name(i);
        if (has_prefix(epath, "../") or epath.find("/../") != StringView::npos) {
            THROW("Found invalid component \"../\" - archive is not safe"
                  " for the further processing as it may be disambiguating");
        }

        add_entry(epath);
    }
}

string zip_package_main_dir(ZipFile& zip) {
    string res;
    auto eno = zip.entries_no();
    for (decltype(eno) i = 0; i < eno; ++i) {
        StringView epath = zip.get_name(i);
        // Check if entry contains ".." component
        if (has_prefix(epath, "../") or epath.find("/../") != StringView::npos) {
            THROW("Found invalid component \"../\" - archive is not safe"
                  " for the further processing as it may be disambiguating");
        }

        if (res.empty()) { // Init res
            auto pos = epath.find('/');
            if (pos == StringView::npos) {
                return "";
            }

            res = epath.substr(0, pos + 1).to_string();
            continue;
        }

        if (not has_prefix(epath, res)) {
            return "";
        }
    }

    return res;
}

} // namespace sim

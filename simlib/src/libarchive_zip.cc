#include "../include/libarchive_zip.hh"
#include "../include/file_manip.hh"
#include "../include/process.hh"
#include "../include/spawner.hh"
#include "../include/temporary_directory.hh"
#include "../include/unlinked_temporary_file.hh"
#include "../include/working_directory.hh"

#if __has_include(<archive.h>) and __has_include(<archive_entry.h>)

template <class Func>
static void
update_add_file_to_zip_impl(Func&& apply_file, StringView new_filename,
                            FilePath zip_filename, bool easy_case = false) {
	constexpr const char* command = "zip";

	Spawner::ExitStat es;
	FileDescriptor zip_output {open_unlinked_tmp_file(O_CLOEXEC)}; /* It isn't a
	    fatal error if zip_output is invalid, so it can be ignored */

	if (easy_case) {
		std::vector<std::string> zip_args;
		back_insert(zip_args, command, "-q", "-r", zip_filename,
		            new_filename.to_string());
		es = Spawner::run(zip_args[0], zip_args, {-1, zip_output, zip_output});

	} else {
		TemporaryDirectory tmpdir("/tmp/zip-update.XXXXXX");
		// Secure new_filename
		auto secured_filename = path_absolute(new_filename);
		throw_assert(new_filename.size() > 0);

		new_filename = StringView(secured_filename, 1); // Trim leading '/'

		auto pos = new_filename.rfind('/');
		// If we have to create directories first
		if (pos != StringView::npos and
		    mkdir_r(concat_tostr(tmpdir.path(), new_filename.substr(0, pos)))) {
			THROW("mkdir() failed", errmsg());
		}

		// Make a symlink
		decltype(get_cwd()) cwd;
		apply_file(cwd, concat(tmpdir.path(), new_filename));

		// Path to zip file is relative we have to make it absolute
		throw_assert(zip_filename.size() > 0);
		if (zip_filename[0] != '/') {
			if (cwd.size == 0)
				cwd = get_cwd();
			cwd.append(zip_filename);
			zip_filename = cwd;
		}

		std::vector<std::string> zip_args;
		back_insert(zip_args, command, "-q", "-r", zip_filename,
		            new_filename.to_string());

		es = Spawner::run(zip_args[0], zip_args,
		                  {
		                     -1,
		                     zip_output,
		                     zip_output,
		                     tmpdir.path(),
		                  });
	}

	if (es.si.code != CLD_EXITED or es.si.status != 0) {
		(void)lseek(zip_output, 0, SEEK_SET);
		THROW("Updating/adding file to zip error: ", es.message, "; ", command,
		      "'s output:\n", get_file_contents(zip_output));
	}
}

void update_add_file_to_zip(FilePath filename, StringView new_filename,
                            FilePath zip_filename) {
	if (new_filename.empty())
		return update_add_file_to_zip_impl(
		   [](auto&&, auto&&) {}, CStringView(filename), zip_filename, true);

	// Remove Trailing '/'
	while (new_filename.size() > 1 and new_filename.back() == '/')
		new_filename.remove_suffix(1);

	return update_add_file_to_zip_impl(
	   [&](decltype(get_cwd())& cwd, FilePath dest_file) {
		   if (new_filename.back() != '/') {
			   throw_assert(filename.size() > 0);
			   // Make filename absolute
			   if (filename[0] != '/') {
				   cwd = get_cwd();
				   auto old_size = cwd.size;
				   cwd.append(filename);
				   filename = cwd;
				   cwd.size = old_size; // Restore cwd to contain only CWD
			   }

			   if (symlink(filename, dest_file))
				   THROW("symlink() failed", errmsg());
		   }
	   },
	   new_filename, zip_filename);
}

void update_add_data_to_zip(StringView data, StringView new_filename,
                            FilePath zip_filename) {
	throw_assert(new_filename.size() > 0);
	return update_add_file_to_zip_impl(
	   [&](decltype(get_cwd())&, FilePath dest_file) {
		   if (CStringView(dest_file).back() != '/') // Dest file is a directory
			   put_file_contents(dest_file, data);
	   },
	   new_filename, zip_filename);
}

#endif // __has_include

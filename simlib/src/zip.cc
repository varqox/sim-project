#include "../include/zip.h"
#include "../include/process.h"
#include "../include/spawner.h"

template<class Func>
static void update_add_file_to_zip_impl(Func&& apply_file,
	StringView new_filename, CStringView zip_filename, bool easy_case = false)
{
	constexpr const char* command = "zip";

	Spawner::ExitStat es;
	FileDescriptor zip_output {openUnlinkedTmpFile()}; /* It isn't a fatal error
		if zip_output is invalid, so it can be ignored */

	if (easy_case) {
		std::vector<std::string> zip_args;
		back_insert(zip_args, command, "-q", "-r", zip_filename.to_string(),
			new_filename.to_string());
		es = Spawner::run(zip_args[0], zip_args, {-1, zip_output, zip_output});

	} else {
		TemporaryDirectory tmpdir("/tmp/zip-update.XXXXXX");
		// Secure new_filename
		auto secured_filename = abspath(new_filename);
		throw_assert(new_filename.size() > 0);

		new_filename = StringView(secured_filename, 1); // Trim leading '/'

		auto pos = new_filename.rfind('/');
		// If we have to create directories first
		if (pos != StringView::npos and mkdir_r(concat_tostr(tmpdir.path(),
			new_filename.substr(0, pos))))
		{
			THROW("mkdir() failed", error());
		}

		// Make a symlink
		decltype(getCWD()) cwd;
		apply_file(cwd, concat(tmpdir.path(), new_filename).to_cstr());

		// Path to zip file is relative we have to make it absolute
		throw_assert(zip_filename.size() > 0);
		if (zip_filename[0] != '/') {
			if (cwd.size == 0)
				cwd = getCWD();
			cwd.append(zip_filename);
			zip_filename = cwd.to_cstr();
		}

		std::vector<std::string> zip_args;
		back_insert(zip_args, command, "-q", "-r", zip_filename.to_string(),
			new_filename.to_string());
		es = Spawner::run(zip_args[0], zip_args, {-1, zip_output, zip_output},
			tmpdir.path());
	}

	if (es.si.code != CLD_EXITED or es.si.status != 0) {
		(void)lseek(zip_output, 0, SEEK_SET);
		THROW("Updating/adding file to zip error: ", es.message, "; ", command,
			"'s output:\n", getFileContents(zip_output));
	}
}

void update_add_file_to_zip(CStringView filename, StringView new_filename,
	CStringView zip_filename)
{
	if (new_filename.empty())
		return update_add_file_to_zip_impl([](auto&&, auto&&){}, filename,
			zip_filename, true);

	// Remove Trailing '/'
	while (new_filename.size() > 1 and new_filename.back() == '/')
		new_filename.removeSuffix(1);

	return update_add_file_to_zip_impl(
		[&](decltype(getCWD())& cwd, CStringView dest_file) {
			if (new_filename.back() != '/') {
				throw_assert(filename.size() > 0);
				// Make filename absolute
				if (filename[0] != '/') {
					cwd = getCWD();
					auto old_size = cwd.size;
					cwd.append(filename);
					filename = cwd.to_cstr();
					cwd.size = old_size; // Restore cwd to contain only CWD
				}

				if (symlink(filename.data(), dest_file.data()))
				{
					THROW("symlink() failed", error());
				}
			}
		}, new_filename, zip_filename);
}

void update_add_data_to_zip(StringView data, StringView new_filename,
	CStringView zip_filename)
{
	throw_assert(new_filename.size() > 0);
		return update_add_file_to_zip_impl(
		[&](decltype(getCWD())&, CStringView dest_file) {
			if (dest_file.back() != '/') // Dest file is a directory
				putFileContents(dest_file, data);
		}, new_filename, zip_filename);
}

#include "../include/temporary_directory.hh"
#include "../include/file_manip.hh"
#include "../include/path.hh"
#include "../include/working_directory.hh"

TemporaryDirectory::TemporaryDirectory(FilePath templ) {
	size_t size = templ.size();
	if (size > 0) {
		// Fill name_
		while (size && templ[size - 1] == '/')
			--size;

		name_.reset(new char[size + 2]);

		memcpy(name_.get(), templ, size);
		name_.get()[size] = name_.get()[size + 1] = '\0';

		// Create directory with permissions (mode: 0700/rwx------)
		if (mkdtemp(name_.get()) == nullptr)
			THROW("Cannot create temporary directory");

		if (name_.get()[0] == '/') { // name_ is absolute
			path_ = name();
		} else { // name_ is not absolute
			path_ = concat_tostr(get_cwd(), name());
		}

		// Make path_ absolute
		path_ = path_absolute(path_);
		if (path_.back() != '/')
			path_ += '/';

		name_.get()[size] = '/';
	}
}

TemporaryDirectory& TemporaryDirectory::operator=(TemporaryDirectory&& td) {
	if (exists() && remove_r(path_) == -1)
		THROW("remove_r() failed", errmsg());

	path_ = std::move(td.path_);
	name_ = std::move(td.name_);
	return *this;
}

TemporaryDirectory::~TemporaryDirectory() {
#ifdef DEBUG
	if (exists() && remove_r(path_) == -1)
		errlog("Error: remove_r()", errmsg()); // We cannot throw because
		                                       // throwing from the destructor
		                                       // is (may be) UB
#else
	if (exists())
		(void)remove_r(path_); // Return value is ignored, we cannot throw
		                       // (because throwing from destructor is (may be)
		                       //   UB), logging it is also not so good
#endif
}

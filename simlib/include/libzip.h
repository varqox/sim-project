#pragma once
#include "filesystem.h"

#include <zip.h>

class ZipError {
	zip_error_t error_;

public:
	ZipError() { zip_error_init(&error_); }

	ZipError(const ZipError&) = delete;
	ZipError& operator=(const ZipError&) = delete;

	ZipError(int ze) { zip_error_init_with_code(&error_, ze); }

	operator zip_error_t*() noexcept { return &error_; }

	const char* str() noexcept { return zip_error_strerror(&error_); }

	~ZipError() { zip_error_fini(&error_); }
};

class ZipEntry {
	friend class ZipFile;

	zip_file_t* zfile_;

public:
	ZipEntry(const ZipEntry&) = delete;
	ZipEntry(ZipEntry&& ze) : zfile_(std::exchange(ze.zfile_, nullptr)) {}

private:
	ZipEntry(zip_file_t *zfile) : zfile_(zfile) {}

	explicit operator bool() const noexcept { return zfile_; }

public:
	ZipEntry& operator=(const ZipEntry&) = delete;

	ZipEntry& operator=(ZipEntry&& ze) {
		close();
		zfile_ = std::exchange(ze.zfile_, nullptr);
		return *this;
	}

	auto read(void* buff, zip_uint64_t nbytes) {
		auto rc = zip_fread(zfile_, buff, nbytes);
		// TODO: check if -1 value prodces a meaningful error
		if (rc == -1)
			THROW("zip_fread() - ", zip_file_strerror(zfile_));

		return rc;
	}

	void close() {
		if (not zfile_)
			return;

		int err = zip_fclose(zfile_);
		zfile_ = nullptr;
		if (err)
			THROW("zip_fclose() - ", ZipError(err).str());
	}

	~ZipEntry() {
		if (zfile_)
			(void)zip_fclose(zfile_);
	}
};

class ZipSource {
	friend class ZipFile;

	zip_source_t* zsource_;

private:
	ZipSource(zip_t* zip, const void* data, zip_uint64_t len)
		: zsource_(zip_source_buffer(zip, data, len, 0))
	{
		if (zsource_ == nullptr)
			THROW("zip_source_buffer() - ", zip_strerror(zip));
	}

	ZipSource(zip_t* zip, const char* fname, zip_uint64_t start, zip_int64_t len)
		: zsource_(zip_source_file(zip, fname, start, len))
	{
		if (zsource_ == nullptr)
			THROW("zip_source_file() - ", zip_strerror(zip));
	}

	ZipSource(zip_t* zip, FILE* file, zip_uint64_t start, zip_int64_t len)
		: zsource_(zip_source_filep(zip, file, start, len))
	{
		if (zsource_ == nullptr)
			THROW("zip_source_filep() - ", zip_strerror(zip));
	}

	ZipSource(zip_t* zip, zip_t* src_zip, zip_uint64_t srcidx, zip_flags_t flags,
			zip_uint64_t start, zip_int64_t len)
		: zsource_(zip_source_zip(zip, src_zip, srcidx, flags, start, len))
	{
		if (zsource_ == nullptr)
			THROW("zip_source_zip() - ", zip_strerror(zip));
	}

public:
	ZipSource(const ZipSource&) = delete;

	ZipSource(ZipSource&& zs) : zsource_(std::exchange(zs.zsource_, nullptr)) {}

	ZipSource& operator=(const ZipSource&) = delete;

	ZipSource& operator=(ZipSource&& zs) {
		free();
		zsource_ = std::exchange(zs.zsource_, nullptr);
		return *this;
	}

	void free() {
		if (zsource_) {
			zip_source_free(zsource_);
			zsource_ = nullptr;
		}
	}

	~ZipSource() { free(); }
};

class ZipFile {
	zip_t* zip_ = nullptr;

public:
	using index_t = zip_int64_t;

	ZipFile() = default;

	ZipFile(FilePath file, int flags = 0) {
		int error;
		zip_ = zip_open(file.data(), flags, &error);
		if (zip_ == nullptr)
			THROW("zip_open() - ", ZipError(error).str());
	}

	ZipFile(const ZipFile&) = delete;

	ZipFile(ZipFile&& zf) noexcept : zip_(std::exchange(zf.zip_, nullptr)) {}

	ZipFile& operator=(const ZipFile&) = delete;

	ZipFile& operator=(ZipFile&& zf) {
		discard();
		zip_ = std::exchange(zf.zip_, nullptr);
		return *this;
	}

	operator zip_t*() & noexcept { return zip_; }

	operator zip_t*() && = delete; // Disallow calling it on temporaries

	// @p fname has to be a full path
	index_t get_index(FilePath fname) & {
		auto index = zip_name_locate(zip_, fname, 0);
		if (index != -1)
			return index;
		if (zip_error_code_zip(zip_get_error(zip_)) == ZIP_ER_NOENT)
			return -1;

		THROW("zip_name_locate() - ", zip_strerror(zip_));
	}

	// @p fname has to be a full path
	bool has_entry(FilePath fname) { return (get_index(fname) != -1); }

	ZipEntry get_entry(index_t index, zip_flags_t flags = 0) {
		ZipEntry entry(zip_fopen_index(zip_, index, flags));
		if (not entry)
			THROW("zip_fopen_index() - ", zip_strerror(zip_));

		return entry;
	}

	// @p fname has to be a full path
	// ZipEntry get_entry(FilePath fname, zip_flags_t flags = 0) {
	// 	ZipEntry entry(zip_fopen(zip_, fname, flags));
	// 	if (not entry)
	// 		THROW("zip_fopen() - ", zip_strerror(zip_));

	// 	return entry;
	// }

	// It invalid to call this method on a closed archive
	index_t entries_no(zip_flags_t flags = 0) noexcept {
		return zip_get_num_entries(zip_, flags);
	}

	void stat(index_t index, zip_stat_t& sb, zip_flags_t flags = 0) {
		if (zip_stat_index(zip_, index, flags, &sb))
			THROW("zip_stat_index() - ", zip_strerror(zip_));
	}

	const char* get_name(index_t index, zip_flags_t flags = 0) {
		const char* res = zip_get_name(zip_, index, flags);
		if (res == NULL)
			THROW("zip_get_name() - ", zip_strerror(zip_));

		return res;
	}

	auto entry_size(zip_uint16_t index) {
		zip_stat_t sb;
		sb.valid = ZIP_STAT_SIZE;
		stat(index, sb);
		return sb.size;
	}

	std::string extract_to_str(index_t index) {
		auto size = entry_size(index);
		std::string res(size, '\0');
		auto entry = get_entry(index);
		decltype(size) pos = 0;
#if __cplusplus > 201402L
#warning "Since C++17 data() method may be used in the below read"
#endif
		while (pos < size)
			pos += entry.read(&res[pos], size - pos);

		return res;
	}

	void extract_to_fd(index_t index, int fd) {
		auto entry = get_entry(index);
		for (;;) {
			constexpr auto BUFF_SIZE = 1 << 16;
			char buff[BUFF_SIZE];
			auto rc = entry.read(buff, BUFF_SIZE);
			if (rc == 0)
				break;

			writeAll_throw(fd, buff, rc);
		}
	}

	void extract_to_file(index_t index, FilePath fpath, mode_t mode = S_0644) {
		FileDescriptor fd(fpath, O_WRONLY | O_CREAT | O_TRUNC, mode);
		if (fd == -1)
			THROW("open()", errmsg());

		return extract_to_fd(index, fd);
	}

	index_t dir_add(FilePath name, zip_flags_t flags = 0) {
		index_t res = zip_dir_add(zip_, name, flags);
		if (res == -1)
			THROW("zip_dir_add() - ", zip_strerror(zip_));

		return res;
	}

	// @p data has to be valid until calling close() (and is not freed)
	ZipSource source_buffer(const void* data, zip_uint64_t len) {
		return ZipSource(zip_, data, len);
	}

	// @p str has to be valid until calling close() (and is not freed)
	ZipSource source_buffer(StringView str) {
		return ZipSource(zip_, str.data(), str.size());
	}

	// By default whole file (len == -1 means reading till the end of file)
	ZipSource source_file(FilePath fpath, zip_uint64_t start = 0, zip_int64_t len = -1) {
		return ZipSource(zip_, fpath, start, len);
	}

	// By default whole file (len == -1 means reading till the end of file)
	// @p file will be closed only on successful close() (as long as I
	// understood the documentation correctly)
	ZipSource source_file(FILE* file, zip_uint64_t start = 0, zip_int64_t len = -1) {
		return ZipSource(zip_, file, start, len);
	}

	// By default whole file (len == -1 means reading till the end of file)
	ZipSource source_zip(ZipFile& src_zip, index_t src_idx,
		zip_flags_t flags = 0, zip_uint64_t start = 0, zip_int64_t len = -1)
	{
		return ZipSource(zip_, src_zip, src_idx, flags, start, len);
	}

	void file_set_compression(index_t index, zip_int32_t method, zip_uint32_t flags) {
		if (zip_set_file_compression(zip_, index, method, flags))
			THROW("zip_set_file_compression() - ", zip_strerror(zip_));
	}

	// If name ends with '/' the directory will be created. If you want to
	// create directory using this method, you should end it with '/' as libzip
	// has some problems if you do not do so (e.g. with source created from zip
	// archive).
	// @p compression_level == 0 means default compression level
	index_t file_add(FilePath name, ZipSource&& source, zip_flags_t flags = 0, zip_uint32_t compression_level = 4) {
		// Directory has to be added via zip_dir_add()
		if (hasSuffix(name.to_cstr(), "/"))
			return dir_add(name, flags);

		index_t res = zip_file_add(zip_, name, source.zsource_, flags);
		if (res == -1)
			THROW("zip_file_add() - ", zip_strerror(zip_));

		source.zsource_ = nullptr;
		if (compression_level > 0)
			file_set_compression(res, ZIP_CM_DEFLATE, compression_level);

		return res;
	}

	// @p compression_level == 0 means default compression level
	void file_replace(index_t index, ZipSource&& source, zip_flags_t flags = 0, zip_uint32_t compression_level = 4) {
		if (zip_file_replace(zip_, index, source.zsource_, flags))
			THROW("zip_file_replace() - ", zip_strerror(zip_));

		source.zsource_ = nullptr;
		if (compression_level > 0)
			file_set_compression(index, ZIP_CM_DEFLATE, compression_level);
	}

	void file_rename(index_t index, FilePath name, zip_flags_t flags = 0) {
		if (zip_file_rename(zip_, index, name, flags))
			THROW("zip_file_rename() - ", zip_strerror(zip_));
	}

	void file_delete(index_t index) {
		if (zip_delete(zip_, index))
			THROW("zip_delete() - ", zip_strerror(zip_));
	}

	// Reverts all changes to the entry @p index
	void file_unchange(index_t index) {
		if (zip_unchange(zip_, index))
			THROW("zip_unchange() - ", zip_strerror(zip_));
	}

	// Reverts all changes in the zip archive
	void unchange_all() {
		if (zip_unchange_all(zip_))
			THROW("zip_unchange_all() - ", zip_strerror(zip_));
	}

	// Reverts all changes to the archive comment and global flags
	void unchange_archive() {
		if (zip_unchange_archive(zip_))
			THROW("zip_unchange_archive() - ", zip_strerror(zip_));
	}

	void close() {
		if (zip_close(zip_))
			THROW("zip_close() - ", zip_strerror(zip_));
		else
			zip_ = nullptr;
	}

	void discard() noexcept {
		if (zip_) {
			zip_discard(zip_);
			zip_ = nullptr;
		}
	}

	~ZipFile() { discard(); }
};

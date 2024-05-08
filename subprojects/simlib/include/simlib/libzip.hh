#pragma once

#include <optional>
#include <simlib/call_in_destructor.hh>
#include <simlib/file_contents.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/macros/stack_unwinding.hh>
#include <simlib/string_traits.hh>
#include <simlib/string_view.hh>
#include <sys/stat.h>
#include <zip.h>

class ZipError {
    zip_error_t error_{};

public:
    ZipError() noexcept { zip_error_init(&error_); }

    ZipError(const ZipError&) = delete;
    ZipError(ZipError&&) = delete;
    ZipError& operator=(const ZipError&) = delete;
    ZipError& operator=(ZipError&&) = delete;

    explicit ZipError(int ze) noexcept { zip_error_init_with_code(&error_, ze); }

    explicit operator zip_error_t*() noexcept { return &error_; }

    const char* str() noexcept { return zip_error_strerror(&error_); }

    ~ZipError() { zip_error_fini(&error_); }
};

class ZipEntry {
    friend class ZipFile;

    zip_file_t* zfile_;

public:
    ZipEntry(const ZipEntry&) = delete;

    ZipEntry(ZipEntry&& ze) noexcept : zfile_(std::exchange(ze.zfile_, nullptr)) {}

private:
    explicit ZipEntry(zip_file_t* zfile) : zfile_(zfile) {}

    explicit operator bool() const noexcept { return zfile_; }

public:
    ZipEntry& operator=(const ZipEntry&) = delete;

    // NOLINTNEXTLINE(performance-noexcept-move-constructor): close() may throw
    ZipEntry& operator=(ZipEntry&& ze) {
        close();
        zfile_ = std::exchange(ze.zfile_, nullptr);
        return *this;
    }

    auto read(void* buff, zip_uint64_t nbytes) {
        auto rc = zip_fread(zfile_, buff, nbytes);
        // TODO: check if -1 value produces a meaningful error
        if (rc == -1) {
            THROW("zip_fread() - ", zip_file_strerror(zfile_));
        }

        return rc;
    }

    void close() {
        if (not zfile_) {
            return;
        }

        int err = zip_fclose(zfile_);
        zfile_ = nullptr;
        if (err) {
            THROW("zip_fclose() - ", ZipError(err).str());
        }
    }

    ~ZipEntry() {
        if (zfile_) {
            (void)zip_fclose(zfile_);
        }
    }
};

class ZipSource {
    friend class ZipFile;

    static constexpr auto default_mode = (S_IFREG | S_0644);

    zip_source_t* zsource_;
    zip_uint8_t opsys_ = ZIP_OPSYS_UNIX;
    zip_uint32_t external_attributes_ = default_mode << 16;

private:
    ZipSource(zip_t* zip, const void* data, zip_uint64_t len)
    : zsource_(zip_source_buffer(zip, data, len, 0)) {
        STACK_UNWINDING_MARK;
        if (zsource_ == nullptr) {
            THROW("zip_source_buffer() - ", zip_strerror(zip));
        }
    }

    ZipSource(zip_t* zip, const char* fname, zip_uint64_t start, zip_int64_t len)
    : zsource_(zip_source_file(zip, fname, start, len)) {
        STACK_UNWINDING_MARK;
        struct stat st = {};
        if (lstat(fname, &st)) {
            THROW("lstat()", errmsg());
        }

        set_file_permissions(st.st_mode);
        if (zsource_ == nullptr) {
            THROW("zip_source_file() - ", zip_strerror(zip));
        }
    }

    ZipSource(zip_t* zip, FILE* file, zip_uint64_t start, zip_int64_t len)
    : zsource_(zip_source_filep(zip, file, start, len)) {
        STACK_UNWINDING_MARK;
        struct stat st = {};
        if (fstat(fileno(file), &st)) {
            THROW("lstat()", errmsg());
        }

        set_file_permissions(st.st_mode);
        if (zsource_ == nullptr) {
            THROW("zip_source_filep() - ", zip_strerror(zip));
        }
    }

// Debian 12 still uses too old version of the libzip to upgrade to the non-deprecated API
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    ZipSource(
        zip_t* zip,
        zip_t* src_zip,
        zip_uint64_t srcidx,
        zip_flags_t flags,
        zip_uint64_t start,
        zip_int64_t len
    )
    : zsource_(zip_source_zip(zip, src_zip, srcidx, flags, start, len)) {
        STACK_UNWINDING_MARK;
        zip_uint8_t opsys = 0;
        zip_uint32_t attrs = 0;
        if (zip_file_get_external_attributes(src_zip, srcidx, 0, &opsys, &attrs)) {
            THROW("zip_file_get_external_attributes() - ", zip_strerror(src_zip));
        }

        opsys_ = opsys;
        external_attributes_ = attrs;

        if (zsource_ == nullptr) {
            THROW("zip_source_zip() - ", zip_strerror(zip));
        }
    }

#pragma GCC diagnostic pop

public:
    ZipSource(const ZipSource&) = delete;

    ZipSource(ZipSource&& zs) noexcept : zsource_(std::exchange(zs.zsource_, nullptr)) {}

    ZipSource& operator=(const ZipSource&) = delete;

    ZipSource& operator=(ZipSource&& zs) noexcept {
        free();
        zsource_ = std::exchange(zs.zsource_, nullptr);
        return *this;
    }

    void set_file_mode(mode_t mode) noexcept {
        opsys_ = ZIP_OPSYS_UNIX;
        external_attributes_ = mode << 16;
    }

    void set_file_permissions(mode_t permissions) noexcept {
        if (opsys_ != ZIP_OPSYS_UNIX) {
            opsys_ = ZIP_OPSYS_UNIX;
            external_attributes_ = default_mode << 16;
        }

        external_attributes_ &= ~(ALLPERMS << 16);
        external_attributes_ |= (permissions & ALLPERMS) << 16;
    }

    [[nodiscard]] std::optional<mode_t> get_file_mode() const noexcept {
        if (opsys_ != ZIP_OPSYS_UNIX) {
            return std::nullopt;
        }

        return external_attributes_ >> 16;
    }

    [[nodiscard]] std::optional<mode_t> get_file_permissions() const noexcept {
        if (opsys_ != ZIP_OPSYS_UNIX) {
            return std::nullopt;
        }

        return (external_attributes_ >> 16) & ALLPERMS;
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

    explicit ZipFile(FilePath file, int flags = 0) {
        STACK_UNWINDING_MARK;
        int error = 0;
        zip_ = zip_open(file.data(), flags, &error);
        if (zip_ == nullptr) {
            THROW("zip_open() - ", ZipError(error).str());
        }
    }

    ZipFile(const ZipFile&) = delete;

    ZipFile(ZipFile&& zf) noexcept : zip_(std::exchange(zf.zip_, nullptr)) {}

    ZipFile& operator=(const ZipFile&) = delete;

    ZipFile& operator=(ZipFile&& zf) noexcept {
        discard();
        zip_ = std::exchange(zf.zip_, nullptr);
        return *this;
    }

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator zip_t*() & noexcept { return zip_; }

    operator zip_t*() && = delete; // Disallow calling it on temporaries

    // @p fname has to be a full path
    index_t get_index(FilePath fname) & {
        STACK_UNWINDING_MARK;
        auto index = zip_name_locate(zip_, fname, 0);
        if (index != -1) {
            return index;
        }
        if (zip_error_code_zip(zip_get_error(zip_)) == ZIP_ER_NOENT) {
            return -1;
        }

        THROW("zip_name_locate() - ", zip_strerror(zip_));
    }

    // @p fname has to be a full path
    bool has_entry(FilePath fname) { return (get_index(fname) != -1); }

    ZipEntry get_entry(index_t index, zip_flags_t flags = 0) {
        STACK_UNWINDING_MARK;
        ZipEntry entry(zip_fopen_index(zip_, index, flags));
        if (not entry) {
            THROW("zip_fopen_index() - ", zip_strerror(zip_));
        }

        return entry;
    }

    // It invalid to call this method on a closed archive
    index_t entries_no(zip_flags_t flags = 0) noexcept { return zip_get_num_entries(zip_, flags); }

    void stat(index_t index, zip_stat_t& sb, zip_flags_t flags = 0) {
        STACK_UNWINDING_MARK;
        if (zip_stat_index(zip_, index, flags, &sb)) {
            THROW("zip_stat_index() - ", zip_strerror(zip_));
        }
    }

    const char* get_name(index_t index, zip_flags_t flags = 0) {
        STACK_UNWINDING_MARK;
        const char* res = zip_get_name(zip_, index, flags);
        if (res == nullptr) {
            THROW("zip_get_name() - ", zip_strerror(zip_));
        }

        return res;
    }

    auto entry_size(zip_uint16_t index) {
        STACK_UNWINDING_MARK;
        zip_stat_t sb;
        sb.valid = ZIP_STAT_SIZE;
        stat(index, sb);
        return sb.size;
    }

    std::string extract_to_str(index_t index) {
        STACK_UNWINDING_MARK;
        auto size = entry_size(index);
        std::string res(size, '\0');
        auto entry = get_entry(index);
        decltype(size) pos = 0;
        while (pos < size) {
            pos += entry.read(res.data() + pos, size - pos);
        }

        return res;
    }

    void extract_to_fd(index_t index, int fd) {
        STACK_UNWINDING_MARK;
        auto entry = get_entry(index);
        for (;;) {
            constexpr auto BUFF_SIZE = 1 << 16;
            char buff[BUFF_SIZE];
            auto rc = entry.read(buff, BUFF_SIZE);
            if (rc == 0) {
                break;
            }

            write_all_throw(fd, buff, rc);
        }
    }

    void extract_to_file(
        index_t index, FilePath fpath, std::optional<mode_t> permissions = std::nullopt
    ) {
        STACK_UNWINDING_MARK;
        mode_t mode = [&]() -> mode_t {
            if (permissions) {
                return *permissions;
            }

            zip_uint8_t opsys = 0;
            zip_uint32_t attrs = 0;
            if (zip_file_get_external_attributes(zip_, index, 0, &opsys, &attrs)) {
                THROW("zip_file_get_external_attributes() - ", zip_strerror(zip_));
            }

            if (opsys != ZIP_OPSYS_UNIX) {
                return S_0644;
            }

            return attrs >> 16;
        }();

        FileDescriptor fd(fpath, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, mode);
        if (fd == -1) {
            THROW("open()", errmsg());
        }

        return extract_to_fd(index, fd);
    }

    index_t dir_add(FilePath name, zip_flags_t flags = 0, mode_t permissions = S_0755) {
        index_t idx = zip_dir_add(zip_, name, flags);
        if (idx == -1) {
            THROW("zip_dir_add() - ", zip_strerror(zip_));
        }

        if (zip_file_set_external_attributes(
                zip_, idx, 0, ZIP_OPSYS_UNIX, (S_IFDIR | (permissions & ALLPERMS)) << 16
            ))
        {
            THROW("zip_file_set_external_attributes() - ", zip_strerror(zip_));
        }

        return idx;
    }

    // @p data has to be valid until calling close() (and is not freed)
    ZipSource source_buffer(const void* data, zip_uint64_t len) { return {zip_, data, len}; }

    // @p str has to be valid until calling close() (and is not freed)
    ZipSource source_buffer(StringView str) { return {zip_, str.data(), str.size()}; }

    // By default whole file (len == -1 means reading till the end of file)
    ZipSource source_file(FilePath fpath, zip_uint64_t start = 0, zip_int64_t len = -1) {
        return {zip_, fpath, start, len};
    }

    // By default whole file (len == -1 means reading till the end of file)
    // @p file will be closed only on successful close() (as long as I
    // understood the documentation correctly)
    ZipSource source_file(FILE* file, zip_uint64_t start = 0, zip_int64_t len = -1) {
        return {zip_, file, start, len};
    }

    // By default whole file (len == -1 means reading till the end of file)
    ZipSource source_zip(
        ZipFile& src_zip,
        index_t src_idx,
        zip_flags_t flags = 0,
        zip_uint64_t start = 0,
        zip_int64_t len = -1
    ) {
        return {zip_, src_zip, static_cast<zip_uint64_t>(src_idx), flags, start, len};
    }

    void file_set_compression(index_t index, zip_int32_t method, zip_uint32_t flags) {
        STACK_UNWINDING_MARK;
        if (zip_set_file_compression(zip_, index, method, flags)) {
            THROW("zip_set_file_compression() - ", zip_strerror(zip_));
        }
    }

    // If name ends with '/' the directory will be created. If you want to
    // create directory using this method, you should end it with '/' as libzip
    // has some problems if you do not do so (e.g. with source created from zip
    // archive).
    // @p compression_level == 0 means default compression level
    index_t file_add(
        FilePath name, ZipSource&& source, zip_flags_t flags = 0, zip_uint32_t compression_level = 4
    ) {
        STACK_UNWINDING_MARK;
        // Directory has to be added via zip_dir_add()
        if (has_suffix(name.to_cstr(), "/")) {
            if (source.opsys_ == ZIP_OPSYS_UNIX) {
                return dir_add(name, flags, source.external_attributes_ >> 16);
            }

            return dir_add(name, flags);
        }

        index_t idx = zip_file_add(zip_, name, source.zsource_, flags);
        if (idx == -1) {
            THROW("zip_file_add() - ", zip_strerror(zip_));
        }

        CallInDtor idx_deleter = [&] { (void)zip_delete(zip_, idx); };

        if (zip_file_set_external_attributes(
                zip_, idx, 0, source.opsys_, source.external_attributes_
            ))
        {
            THROW("zip_file_set_external_attributes() - ", zip_strerror(zip_));
        }

        source.zsource_ = nullptr;
        if (compression_level > 0) {
            file_set_compression(idx, ZIP_CM_DEFLATE, compression_level);
        }

        idx_deleter.cancel();
        return idx;
    }

    // @p compression_level == 0 means default compression level
    void file_replace(
        index_t index, ZipSource&& source, zip_flags_t flags = 0, zip_uint32_t compression_level = 4
    ) {
        STACK_UNWINDING_MARK;
        if (zip_file_replace(zip_, index, source.zsource_, flags)) {
            THROW("zip_file_replace() - ", zip_strerror(zip_));
        }

        if (zip_file_set_external_attributes(
                zip_, index, 0, source.opsys_, source.external_attributes_
            ))
        {
            THROW("zip_file_set_external_attributes() - ", zip_strerror(zip_));
        }

        source.zsource_ = nullptr;
        if (compression_level > 0) {
            file_set_compression(index, ZIP_CM_DEFLATE, compression_level);
        }
    }

    void file_rename(index_t index, FilePath name, zip_flags_t flags = 0) {
        STACK_UNWINDING_MARK;
        if (zip_file_rename(zip_, index, name, flags)) {
            THROW("zip_file_rename() - ", zip_strerror(zip_));
        }
    }

    void file_delete(index_t index) {
        STACK_UNWINDING_MARK;
        if (zip_delete(zip_, index)) {
            THROW("zip_delete() - ", zip_strerror(zip_));
        }
    }

    // Reverts all changes to the entry @p index
    void file_unchange(index_t index) {
        STACK_UNWINDING_MARK;
        if (zip_unchange(zip_, index)) {
            THROW("zip_unchange() - ", zip_strerror(zip_));
        }
    }

    // Reverts all changes in the zip archive
    void unchange_all() {
        STACK_UNWINDING_MARK;
        if (zip_unchange_all(zip_)) {
            THROW("zip_unchange_all() - ", zip_strerror(zip_));
        }
    }

    // Reverts all changes to the archive comment and global flags
    void unchange_archive() {
        STACK_UNWINDING_MARK;
        if (zip_unchange_archive(zip_)) {
            THROW("zip_unchange_archive() - ", zip_strerror(zip_));
        }
    }

    void close() {
        STACK_UNWINDING_MARK;
        if (zip_close(zip_)) {
            THROW("zip_close() - ", zip_strerror(zip_));
        }
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

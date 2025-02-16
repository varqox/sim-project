#pragma once

#include <atomic>
#include <cstddef>
#include <fcntl.h>
#include <mutex>
#include <optional>
#include <sim/mysql/mysql.hh>
#include <simlib/always_false.hh>
#include <simlib/concat.hh>
#include <simlib/enum_val.hh>
#include <simlib/errmsg.hh>
#include <simlib/file_path.hh>
#include <simlib/inplace_array.hh>
#include <simlib/inplace_buff.hh>
#include <simlib/macros/stack_unwinding.hh>
#include <simlib/macros/throw.hh>
#include <simlib/meta/is_one_of.hh>
#include <simlib/string_view.hh>
#include <simlib/throw_assert.hh>
#include <tuple>
#include <type_traits>
#include <utility>

#if 0
#warning "Before committing disable this debug"
#define DEBUG_MYSQL(...) __VA_ARGS__
#define NO_DEBUG_MYSQL(...)
#else
#define DEBUG_MYSQL(...)
#define NO_DEBUG_MYSQL(...) __VA_ARGS__
#endif

namespace old_mysql {

// Optional type used to access data retrieved from MySQL
template <class T>
class Optional {
    using StoredType = std::remove_const_t<T>;
    StoredType value_{}; // This optional always needs to contain value
    char has_no_value_{true};

    friend class Statement;

public:
    Optional() = default;

    Optional(const Optional&) = default;
    Optional(Optional&&) noexcept = default;

    Optional& operator=(const Optional& other) {
        // As optional may hold InplaceBuff<>, changing its value when it is bounded is
        // dangerous (may result in buffer overflow, as we store in MYSQL_BIND its max_size()
        // along with data pointer which may become invalid after the assignment). Therefore
        // only trivially copyable types are allowed to be assigned
        static_assert(std::is_trivially_copyable_v<T>);
        value_ = other.value_;
        has_no_value_ = other.has_no_value_;
        return *this;
    }

    Optional& operator=(Optional&&) = delete;

    Optional& operator=(std::nullopt_t /*unused*/) noexcept {
        has_no_value_ = true;
        return *this;
    }

    void reset() noexcept { has_no_value_ = true; }

    ~Optional() = default;

    // NOLINTNEXTLINE(google-explicit-constructor)
    Optional(const T& value) : value_(value), has_no_value_(false) {}

    // NOLINTNEXTLINE(google-explicit-constructor)
    Optional(T&& value) : value_(std::move(value)), has_no_value_(false) {}

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator std::optional<T>() const { return to_opt(); }

    template <class U = T, std::enable_if_t<is_enum_val<U>, int> = 0>
    // NOLINTNEXTLINE(google-explicit-constructor)
    operator std::optional<typename U::EnumType>() const {
        return has_no_value_ ? std::optional<T>() : value_;
    }

    [[nodiscard]] std::optional<T> to_opt() const {
        return (has_no_value_ ? std::nullopt : std::optional<T>(value_));
    }

    constexpr explicit operator bool() const noexcept { return not has_no_value_; }

    constexpr const T& operator*() const { return value_; }

    constexpr const T* operator->() const { return std::addressof(value_); }

    [[nodiscard]] constexpr bool has_value() const noexcept { return not has_no_value_; }

    [[nodiscard]] constexpr const T& value() const noexcept {
        assert(not has_no_value_);
        return value_;
    }

    template <class U>
    constexpr T value_or(U&& default_value) const {
        return (has_no_value_ ? std::forward<U>(default_value) : value_);
    }
};

template <class...>
constexpr inline bool is_optional = false;
template <class T>
constexpr inline bool is_optional<Optional<T>> = true;

} // namespace old_mysql

#if __has_include(<mysql.h>) && __has_include(<errmsg.h>)
#define SIMLIB_MYSQL_ENABLED 1
#include <errmsg.h>
#include <mysql.h>

namespace old_mysql {

class ConnectionView;

class Result {
private:
    friend ConnectionView;

    size_t* connection_referencing_objects_no_;
    MYSQL_RES* res_;
    MYSQL_ROW row_{};
    // NOLINTNEXTLINE(google-runtime-int)
    unsigned long* lengths_{};

    Result(MYSQL_RES* res, size_t* conn_ref_objs_no)
    : connection_referencing_objects_no_(conn_ref_objs_no)
    , res_(res) {
        ++*connection_referencing_objects_no_;
    }

public:
    Result(const Result&) = delete;

    Result(Result&& r) noexcept
    : connection_referencing_objects_no_(
          std::exchange(r.connection_referencing_objects_no_, nullptr)
      )
    , res_(std::exchange(r.res_, nullptr))
    , row_(r.row_)
    , lengths_(r.lengths_) {
        // r.row_ = {};
        // r.lengths_ does not have to be changed
    }

    Result& operator=(const Result&) = delete;

    Result& operator=(Result&& r) noexcept {
        if (res_) {
            mysql_free_result(res_);
        }
        if (connection_referencing_objects_no_) {
            --*connection_referencing_objects_no_;
        }

        connection_referencing_objects_no_ =
            std::exchange(r.connection_referencing_objects_no_, nullptr);
        res_ = std::exchange(r.res_, nullptr);
        row_ = r.row_;
        lengths_ = r.lengths_;

        // r.row_ = {};
        // r.lengths_ does not have to be changed
        return *this;
    }

    ~Result() {
        if (res_) {
            mysql_free_result(res_);
        }
        if (connection_referencing_objects_no_) {
            --*connection_referencing_objects_no_;
        }
    }

    MYSQL_RES* impl() noexcept { return res_; }

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator MYSQL_RES*() noexcept { return impl(); }

    bool next() noexcept {
        return (row_ = mysql_fetch_row(res_)) and (lengths_ = mysql_fetch_lengths(res_));
    }

    unsigned fields_num() noexcept { return mysql_num_fields(res_); }

    my_ulonglong rows_num() noexcept { return mysql_num_rows(res_); }

    bool is_null(unsigned idx) NO_DEBUG_MYSQL(noexcept) {
        DEBUG_MYSQL(throw_assert(idx < fields_num());)
        return (row_[idx] == nullptr);
    }

    StringView operator[](unsigned idx) {
        STACK_UNWINDING_MARK;
        DEBUG_MYSQL(throw_assert(idx < fields_num());)
        if (is_null(idx)) {
            THROW("Encountered NULL at column ", idx, " that did not expect nullable data");
        }

        return {row_[idx], lengths_[idx]};
    }

    Optional<StringView> to_opt(unsigned idx) NO_DEBUG_MYSQL(noexcept) {
        if (is_null(idx)) {
            return {};
        };

        return {(*this)[idx]};
    }
};

class Statement {
private:
    friend ConnectionView;

    size_t* connection_referencing_objects_no_ = nullptr;
    MYSQL_STMT* stmt_ = nullptr;

    size_t binds_size_ = 0;
    // Needs to be allocated dynamically to keep binds valid after move
    // construct / assignment
    std::unique_ptr<MYSQL_BIND[]> binds_;

    size_t res_binds_size_ = 0;
    // Needs to be allocated dynamically to keep res binds valid after move
    // construct / assignment
    std::unique_ptr<MYSQL_BIND[]> res_binds_;
    std::unique_ptr<InplaceBuffBase*[]> inplace_buffs_;

    InplaceArray<std::unique_ptr<char[]>, 8> buffs_;

    void clear_bind(unsigned idx) noexcept { memset(&binds_[idx], 0, sizeof(binds_[idx])); }

    void clear_res_bind(unsigned idx) noexcept {
        memset(&res_binds_[idx], 0, sizeof(res_binds_[idx]));
        res_binds_[idx].error = &res_binds_[idx].error_value;
        res_binds_[idx].is_null = &res_binds_[idx].is_null_value;
    }

    Statement(MYSQL_STMT* stmt, size_t* conn_ref_objs_no)
    : connection_referencing_objects_no_(conn_ref_objs_no)
    , stmt_(stmt)
    , binds_size_(mysql_stmt_param_count(stmt))
    , binds_(std::make_unique<MYSQL_BIND[]>(binds_size_))
    , res_binds_size_(mysql_stmt_field_count(stmt))
    , res_binds_(std::make_unique<MYSQL_BIND[]>(res_binds_size_))
    , inplace_buffs_(std::make_unique<InplaceBuffBase*[]>(res_binds_size_)) {

        for (unsigned i = 0; i < binds_size_; ++i) {
            clear_bind(i);
        }

        for (unsigned i = 0; i < res_binds_size_; ++i) {
            clear_res_bind(i);
        }

        ++*connection_referencing_objects_no_;
    }

public:
    Statement() = default;
    Statement(const Statement&) = delete;

    Statement(Statement&& s) noexcept
    : connection_referencing_objects_no_(
          std::exchange(s.connection_referencing_objects_no_, nullptr)
      )
    , stmt_(std::exchange(s.stmt_, nullptr))
    , binds_size_(std::exchange(s.binds_size_, 0))
    , binds_(std::move(s.binds_))
    , res_binds_size_(std::exchange(s.res_binds_size_, 0))
    , res_binds_(std::move(s.res_binds_))
    , inplace_buffs_(std::move(s.inplace_buffs_)) {}

    Statement& operator=(const Statement&) = delete;

    Statement& operator=(Statement&& s) noexcept {
        if (stmt_) {
            (void)mysql_stmt_close(stmt_);
        }
        if (connection_referencing_objects_no_) {
            --*connection_referencing_objects_no_;
        }

        connection_referencing_objects_no_ =
            std::exchange(s.connection_referencing_objects_no_, nullptr);
        stmt_ = std::exchange(s.stmt_, nullptr);
        binds_size_ = std::exchange(s.binds_size_, 0);
        binds_ = std::move(s.binds_);
        res_binds_size_ = std::exchange(s.res_binds_size_, 0);
        res_binds_ = std::move(s.res_binds_);
        inplace_buffs_ = std::move(s.inplace_buffs_);

        return *this;
    }

    ~Statement() {
        if (stmt_) {
            (void)mysql_stmt_close(stmt_);
        }

        if (connection_referencing_objects_no_) {
            --*connection_referencing_objects_no_;
        }
    }

    MYSQL_STMT* impl() noexcept { return stmt_; }

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator MYSQL_STMT*() noexcept { return impl(); }

private:
    template <class T>
    static constexpr auto mysql_type = []() -> decltype(MYSQL_BIND::buffer_type) {
        // NOLINTNEXTLINE(google-runtime-int)
        if constexpr (meta::is_one_of<T, signed char, unsigned char>) {
            return MYSQL_TYPE_TINY;
            // NOLINTNEXTLINE(google-runtime-int)
        } else if constexpr (meta::is_one_of<T, signed short, unsigned short>) {
            return MYSQL_TYPE_SHORT;
            // NOLINTNEXTLINE(google-runtime-int)
        } else if constexpr (meta::is_one_of<T, int, unsigned int>) {
            return MYSQL_TYPE_LONG;
            // NOLINTNEXTLINE(google-runtime-int)
        } else if constexpr (meta::is_one_of<T, long long, unsigned long long>) {
            return MYSQL_TYPE_LONGLONG;
            // NOLINTNEXTLINE(google-runtime-int)
        } else if constexpr (meta::is_one_of<T, signed long, unsigned long>) {
            // NOLINTNEXTLINE(google-runtime-int)
            if constexpr (sizeof(long) == sizeof(int)) {
                return MYSQL_TYPE_LONG;
                // NOLINTNEXTLINE(google-runtime-int)
            } else if constexpr (sizeof(long) == sizeof(long long)) {
                return MYSQL_TYPE_LONGLONG;
            } else {
                static_assert(always_false<T>, "There is no equivalent for type long");
            }
        } else {
            static_assert(always_false<T>, "Invalid type T");
        }
    }();

public:
    /* bind() need to be called before execute() */

    template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
    void bind(unsigned idx, T& x) NO_DEBUG_MYSQL(noexcept) {
        DEBUG_MYSQL(throw_assert(idx < binds_size_);)
        binds_[idx].buffer_type = mysql_type<T>;
        binds_[idx].buffer = &x;
        binds_[idx].is_unsigned = std::is_unsigned_v<T>;
    }

    template <class T, std::enable_if_t<std::is_integral_v<typename T::int_type>, int> = 0>
    void bind(unsigned idx, T& x) NO_DEBUG_MYSQL(noexcept) {
        bind(idx, static_cast<typename T::int_type&>(x));
    }

    void bind(unsigned idx, char* str, size_t& length, size_t max_size) NO_DEBUG_MYSQL(noexcept) {
        DEBUG_MYSQL(throw_assert(idx < binds_size_);)
        clear_bind(idx);
        binds_[idx].buffer_type = MYSQL_TYPE_BLOB;
        binds_[idx].buffer = str;
        binds_[idx].buffer_length = max_size;
        binds_[idx].length = &length;
    }

private:
    void bind(unsigned idx, char* str) NO_DEBUG_MYSQL(noexcept) {
        DEBUG_MYSQL(throw_assert(idx < binds_size_);)
        auto len = std::char_traits<char>::length(str);
        clear_bind(idx);
        binds_[idx].buffer_type = MYSQL_TYPE_BLOB;
        binds_[idx].buffer = (len == 0 ? const_cast<char*>("") : str);
        binds_[idx].buffer_length = len;
        binds_[idx].length = nullptr;
        binds_[idx].length_value = len;
    }

public:
    void bind(unsigned idx, StringBase<char> str) NO_DEBUG_MYSQL(noexcept) {
        DEBUG_MYSQL(throw_assert(idx < binds_size_);)
        clear_bind(idx);
        binds_[idx].buffer_type = MYSQL_TYPE_BLOB;
        binds_[idx].buffer = (str.size() == 0 ? const_cast<char*>("") : str.data());
        binds_[idx].buffer_length = str.size();
        binds_[idx].length = nullptr;
        binds_[idx].length_value = str.size();
    }

    // Like bind(StringView) but copies string into internal buffer - useful
    // for temporary strings
    void bind_copy(unsigned idx, StringView str) {
        DEBUG_MYSQL(throw_assert(idx < binds_size_);)
        auto ptr = std::make_unique<char[]>(str.size() + str.empty());
        //                            Avoid 0-size allocation ^
        std::copy(str.begin(), str.end(), ptr.get());
        StringBase<char> saved_str = {ptr.get(), str.size()};
        buffs_.emplace_back(std::move(ptr));

        bind(idx, saved_str.data());
    }

    template <class T, std::enable_if_t<not std::is_lvalue_reference_v<T>, int> = 0>
    void bind(unsigned idx, T&&) = delete; // Temporaries are NOT safe

    void bind(unsigned idx, std::nullptr_t) NO_DEBUG_MYSQL(noexcept) {
        DEBUG_MYSQL(throw_assert(idx < binds_size_);)
        clear_bind(idx);
        binds_[idx].buffer_type = MYSQL_TYPE_NULL;
    }

private: // For use only by bind_and_execute()
    /// Binds optional value @p x - std::nullopt corresponds to NULL
    template <class T>
    void bind(unsigned idx, std::optional<T>& x) NO_DEBUG_MYSQL(noexcept) {
        if (x.has_value()) {
            bind(idx, *x);
        } else {
            bind(idx, nullptr);
        }
    }

public:
    template <class T>
    void bind(unsigned idx, EnumVal<T>& x) NO_DEBUG_MYSQL(noexcept) {
        bind(idx, static_cast<typename EnumVal<T>::ValType&>(x));
    }

    void fix_binds() {
        if (mysql_stmt_bind_param(stmt_, binds_.get())) {
            THROW(mysql_stmt_error(stmt_));
        }
    }

    template <class... Args>
    void bind_all(Args&&... args) {
        unsigned idx = 0;
        (bind(idx++, std::forward<Args>(args)), ...);
        fix_binds();
    }

    void execute() {
        if (mysql_stmt_execute(stmt_) or mysql_stmt_store_result(stmt_)) {
            THROW(mysql_stmt_error(stmt_));
        }
    }

    void fix_and_execute() {
        fix_binds();
        execute();
    }

    template <class... Args>
    void bind_and_execute(Args&&... args) {
        auto transform_arg = [](auto&& arg) -> decltype(auto) {
            using Type = decltype(arg);
            using TypeNoRef = std::remove_reference_t<Type>;
            using TypeNoRefNoCV = std::remove_cv_t<TypeNoRef>;
            using ArrayType = std::remove_extent_t<TypeNoRef>;

            constexpr size_t inplace_buff_size = 32;

            if constexpr (std::is_same_v<TypeNoRefNoCV, bool>) {
                return static_cast<unsigned char>(arg);
            } else if constexpr (std::is_pointer_v<Type> and std::is_same_v<TypeNoRef, const char*>)
            {
                return InplaceBuff<inplace_buff_size>(arg);
            } else if constexpr (std::is_array_v<TypeNoRef> and
                                 std::is_same_v<ArrayType, const char>)
            {
                return InplaceBuff<inplace_buff_size>(arg);
            } else if constexpr (std::is_base_of_v<StringBase<const char>, TypeNoRefNoCV>) {
                return InplaceBuff<inplace_buff_size>(arg);
            } else if constexpr (std::is_same_v<TypeNoRef, const std::string>) {
                return InplaceBuff<inplace_buff_size>(arg);
            } else if constexpr (old_mysql::is_optional<TypeNoRefNoCV>) {
                return arg.to_opt();
            } else if constexpr (std::is_enum_v<TypeNoRefNoCV>) {
                return EnumVal{arg};
            } else if constexpr (std::is_const_v<TypeNoRef>) {
                return TypeNoRefNoCV(arg); // We need a reference to a non-const
            } else {
                return std::forward<decltype(arg)>(arg);
            }
        };

        [this](auto&&... impl_args) {
            bind_all(impl_args...); // std::forward is intentionally omitted to
                                    // make bind_all() think that all arguments
                                    // are references
            execute();
        }(transform_arg(std::forward<Args>(args))...);
        (void)transform_arg; // Disable GCC warning
    }

private:
    template <class... Params, size_t... I>
    void do_bind_and_execute_from_tuple(
        const std::tuple<Params...>& params, std::index_sequence<I...> /*unused*/
    ) {
        bind_and_execute(std::get<I>(params)...);
    }

public:
    template <class... Params>
    void bind_and_execute_from_tuple(const std::tuple<Params...>& params) {
        do_bind_and_execute_from_tuple(params, std::make_index_sequence<sizeof...(Params)>{});
    }

    /* res_bind() need to be called before next() */

    template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
    void res_bind(unsigned idx, T& x) NO_DEBUG_MYSQL(noexcept) {
        DEBUG_MYSQL(throw_assert(idx < res_binds_size_);)
        res_binds_[idx].buffer_type = mysql_type<T>;
        res_binds_[idx].buffer = &x;
        res_binds_[idx].is_unsigned = std::is_unsigned_v<T>;
    }

    template <class T, std::enable_if_t<std::is_integral_v<typename T::int_type>, int> = 0>
    void res_bind(unsigned idx, T& x) NO_DEBUG_MYSQL(noexcept) {
        res_bind(idx, static_cast<typename T::int_type&>(x));
    }

    void res_bind(unsigned idx, InplaceBuffBase& buff) NO_DEBUG_MYSQL(noexcept) {
        DEBUG_MYSQL(throw_assert(idx < res_binds_size_);)
        clear_res_bind(idx);
        res_binds_[idx].buffer_type = MYSQL_TYPE_BLOB;
        res_binds_[idx].buffer = buff.data();
        res_binds_[idx].buffer_length = buff.max_size();
        res_binds_[idx].length = &buff.size;
        inplace_buffs_[idx] = &buff;
    }

    void res_bind(unsigned idx, std::nullptr_t) NO_DEBUG_MYSQL(noexcept) {
        DEBUG_MYSQL(throw_assert(idx < res_binds_size_);)
        clear_res_bind(idx);
        res_binds_[idx].buffer_type = MYSQL_TYPE_NULL;
    }

    /// Binds optional value @p x - std::nullopt corresponds to NULL
    template <class T>
    void res_bind(unsigned idx, old_mysql::Optional<T>& x) NO_DEBUG_MYSQL(noexcept) {
        res_bind(idx, x.value_);
        static_assert(std::
                          is_same_v<decltype(res_binds_[idx].is_null), decltype(&x.has_no_value_)>);
        res_binds_[idx].is_null = &x.has_no_value_;
    }

    template <class T>
    void res_bind(unsigned idx, EnumVal<T>& x) NO_DEBUG_MYSQL(noexcept) {
        res_bind(idx, static_cast<typename EnumVal<T>::ValType&>(x));
    }

    void res_fix_binds() {
        if (mysql_stmt_bind_result(stmt_, res_binds_.get())) {
            THROW(mysql_stmt_error(stmt_));
        }
    }

    template <class... Args>
    void res_bind_all(Args&&... args) {
        unsigned idx = 0;
        (res_bind(idx++, std::forward<Args>(args)), ...);
        res_fix_binds();
    }

    bool is_null(unsigned idx) NO_DEBUG_MYSQL(noexcept) {
        DEBUG_MYSQL(throw_assert(idx < res_binds_size_);)
        if (res_binds_[idx].buffer_type == MYSQL_TYPE_NULL) {
            return true;
        }

        return (res_binds_[idx].is_null and *res_binds_[idx].is_null);
    }

    bool next() {
        STACK_UNWINDING_MARK;

        res_fix_binds(); // If the previous fetch changed binds we need to rebind the parameters
        int rc = mysql_stmt_fetch(stmt_);
        if (rc == 0) {
            // TODO: this should be checked if trunaction occurs
            // Check for unexpected nulls
            for (unsigned idx = 0; idx < res_binds_size_; ++idx) {
                auto is_null = res_binds_[idx].is_null;
                if (is_null and *is_null and is_null == &res_binds_[idx].is_null_value) {
                    THROW("Encountered NULL at column ", idx, " that did not expect nullable data");
                }
            }

            return true;
        }

        if (rc == MYSQL_NO_DATA) {
            mysql_stmt_free_result(stmt_);
            return false;
        }

        if (rc != MYSQL_DATA_TRUNCATED) {
            THROW(mysql_stmt_error(stmt_));
        }

        // Truncation occurred
        for (unsigned idx = 0; idx < res_binds_size_; ++idx) {
            if (not*res_binds_[idx].error) {
                continue;
            }

            if (res_binds_[idx].buffer_type != MYSQL_TYPE_BLOB) {
                THROW("Truncated data at column ", idx);
            }

            auto& buff = *inplace_buffs_[idx];
            buff.lossy_resize(buff.size);
            res_binds_[idx].buffer = buff.data();
            res_binds_[idx].buffer_length = buff.max_size();

            if (mysql_stmt_fetch_column(stmt_, &res_binds_[idx], idx, 0)) {
                THROW(mysql_stmt_error(stmt_));
            }
        }

        return true;
    }

    unsigned fields_num() noexcept { return mysql_stmt_field_count(stmt_); }

    my_ulonglong rows_num() noexcept { return mysql_stmt_num_rows(stmt_); }

    my_ulonglong insert_id() noexcept { return mysql_stmt_insert_id(stmt_); }

    my_ulonglong affected_rows() noexcept { return mysql_stmt_affected_rows(stmt_); }

    std::unique_ptr<MYSQL_RES, decltype(&mysql_free_result)> res_metadata() noexcept {
        return {mysql_stmt_result_metadata(stmt_), &mysql_free_result};
    }
};

class ConnectionView {
    MYSQL* conn_;
    size_t* connection_referencing_objects_num;

public:
    explicit ConnectionView(::sim::mysql::Connection& conn) noexcept
    : conn_{conn.conn}
    , connection_referencing_objects_num{&conn.referencing_objects_num} {
        ++*connection_referencing_objects_num;
    }

    ~ConnectionView() { --*connection_referencing_objects_num; }

    ConnectionView(const ConnectionView&) = delete;
    ConnectionView(ConnectionView&&) = delete;
    ConnectionView& operator=(const ConnectionView&) = delete;
    ConnectionView& operator=(ConnectionView&&) = delete;

    template <class... Args, std::enable_if_t<(is_string_argument<Args> and ...), int> = 0>
    [[nodiscard]] Result query(Args&&... sql) {
        STACK_UNWINDING_MARK;
        auto sql_str = concat(std::forward<Args>(sql)...);
        if (mysql_real_query(conn_, sql_str.data(), sql_str.size)) {
            THROW(mysql_error(conn_));
        }

        auto* res = mysql_store_result(conn_);
        if (!res) {
            THROW(mysql_error(conn_));
        }
        return {res, connection_referencing_objects_num};
    }

    template <class... Args, std::enable_if_t<(is_string_argument<Args> and ...), int> = 0>
    void update(Args&&... sql) {
        STACK_UNWINDING_MARK;
        auto sql_str = concat(std::forward<Args>(sql)...);
        if (mysql_real_query(conn_, sql_str.data(), sql_str.size)) {
            THROW(mysql_error(conn_));
        }
    }

    template <class... Args, std::enable_if_t<(is_string_argument<Args> and ...), int> = 0>
    [[nodiscard]] Statement prepare(Args&&... sql) {
        STACK_UNWINDING_MARK;
        auto sql_str = concat(std::forward<Args>(sql)...);
        auto* stmt = mysql_stmt_init(conn_);
        if (!stmt) {
            THROW(mysql_error(conn_));
        }
        try {
            if (mysql_stmt_prepare(stmt, sql_str.data(), sql_str.size)) {
                THROW(mysql_error(conn_));
            }
        } catch (...) {
            (void)mysql_stmt_close(stmt);
            throw;
        }
        return {stmt, connection_referencing_objects_num};
    }

    [[nodiscard]] MYSQL* impl() const noexcept { return conn_; }

    my_ulonglong insert_id() noexcept { return mysql_insert_id(conn_); }
};

} // namespace old_mysql

#endif // __has_include(<mysql.h>) && __has_include(<errmsg.h>)

namespace sim::old_mysql {

template <class T>
using Optional = ::old_mysql::Optional<T>;

using Result = ::old_mysql::Result;
using Statement = ::old_mysql::Statement;
using ConnectionView = ::old_mysql::ConnectionView;

} // namespace sim::old_mysql

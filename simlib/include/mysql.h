#pragma once

#include "debug.h"
#include "optional.h"
#include "string.h"
#include "utilities.h"

#include <atomic>
#include <cstddef>
#include <fcntl.h>
#include <functional>
#include <mutex>

#if __has_include(<mysql.h>) && __has_include(<errmsg.h>)
#define SIMLIB_MYSQL_ENABLED 1
#include <errmsg.h>
#include <mysql.h>

#if 0
# warning "Before committing disable this debug"
# define DEBUG_MYSQL(...) __VA_ARGS__
#else
# define DEBUG_MYSQL(...)
#endif

#if __cplusplus > 201402L
#warning "Since C++17 inline constexpr variables and constexpr if will handle this"
#endif

template<class...>
struct is_enum_val : std::false_type {};

template<class T>
struct is_enum_val<EnumVal<T>> : std::true_type {};

template<class T>
struct EnumValTypeHelper {
	using type = void;
};

template<class T>
struct EnumValTypeHelper<EnumVal<T>> {
	using type = T;
};

template<class T>
using EnumValType = typename EnumValTypeHelper<T>::type;

namespace MySQL {

// Optional type used to access data retrieved from MySQL
template<class T>
class Optional {
	using StoredType = std::remove_const_t<T>;
	StoredType value_; // This optional always need to contain value
	my_bool has_no_value_;

	template<size_t, size_t>
	friend class Statement;

public:
	Optional() : has_no_value_(true) {}

	Optional(const Optional&) = default;
	Optional(Optional&&) = default;

	// As optional may hold InplaceBuff<>, changing its value when it is bounded is dangerous (may result in buffer overflow, as we store in MYSQL_BIND its max_size() along with data pointer which become invalid after the assignment)
	Optional& operator=(const Optional&) = delete;
	Optional& operator=(Optional&&) = delete;

	Optional(const T& value) : value_(value), has_no_value_(false) {}

	Optional(T&& value) : value_(std::move(value)), has_no_value_(false) {}

	operator ::Optional<T>() const { return opt(); }

	template<class U = T>
	operator std::enable_if_t<is_enum_val<U>::value, ::Optional<EnumValType<T>>>() const {
		return has_no_value_ ? ::Optional<T>() : value_;
	}

	::Optional<T> opt() const { return has_no_value_ ? ::Optional<T>() : value_; }

	constexpr explicit operator bool() const noexcept { return not has_no_value_; }

	constexpr const T& operator*() const { return value_; }

	constexpr const T* operator->() const { return std::addressof(value_); }

	constexpr bool has_value() const noexcept { return not has_no_value_; }

	constexpr const T& value() const {
		if (has_no_value_)
			THROW("bad optional access");

		return value_;
	}

	template<class U>
	constexpr T value_or(U&& default_value) const {
		return (has_no_value_ ? std::forward<U>(default_value) : value_);
	}
};

class Connection;

class Result {
private:
	friend Connection;

	size_t* connection_referencing_objects_no_;
	MYSQL_RES* res_;
	MYSQL_ROW row_ {};
	unsigned long* lengths_ {};

	Result(MYSQL_RES* res, size_t& conn_ref_objs_no)
		: connection_referencing_objects_no_(&conn_ref_objs_no), res_(res)
	{
		++conn_ref_objs_no;
	}

	Result(const Result&) = delete;
	Result& operator=(const Result&) = delete;

public:
	Result(Result&& r) noexcept
		: connection_referencing_objects_no_(
			std::exchange(r.connection_referencing_objects_no_, nullptr)),
		res_(std::exchange(r.res_, nullptr)), row_(std::move(r.row_)),
		lengths_(r.lengths_)
	{
		// r.row_ = {};
		// r.lengths_ does not have to be changed
	}

	Result& operator=(Result&& r) {
		if (res_)
			mysql_free_result(res_);
		if (connection_referencing_objects_no_)
			--*connection_referencing_objects_no_;

		connection_referencing_objects_no_ =
			std::exchange(r.connection_referencing_objects_no_, nullptr);
		res_ = std::exchange(r.res_, nullptr);
		row_ = std::move(r.row_);
		lengths_ = r.lengths_;

		// r.row_ = {};
		// r.lengths_ does not have to be changed
		return *this;
	}


	~Result() {
		if (res_)
			mysql_free_result(res_);
		if (connection_referencing_objects_no_)
			--*connection_referencing_objects_no_;
	}

	MYSQL_RES* impl() noexcept { return res_; }

	operator MYSQL_RES* () noexcept { return impl(); }

	bool next() noexcept {
		if ((row_ = mysql_fetch_row(res_)) and
			(lengths_ = mysql_fetch_lengths(res_)))
		{
			return true;
		}

		return false;
	}

	unsigned fields_num() noexcept { return mysql_num_fields(res_); }

	my_ulonglong rows_num() noexcept { return mysql_num_rows(res_); }

	bool is_null(unsigned idx) ND(noexcept) {
		D(throw_assert(idx < fields_num());)
		return (row_[idx] == nullptr);
	}

	StringView operator[](unsigned idx) ND(noexcept) {
		D(throw_assert(idx < fields_num());)
		return {row_[idx], lengths_[idx]};
	}

	Optional<StringView> opt(unsigned idx) ND(noexcept) {
		if (is_null(idx))
			return {};

		return {(*this)[idx]};
	}
};

template<size_t STATIC_BINDS = 16, size_t STATIC_RESULTS = 16>
class Statement {
private:
	friend Connection;

	size_t* connection_referencing_objects_no_;
	MYSQL_STMT* stmt_;
	InplaceArray<MYSQL_BIND, STATIC_BINDS> params_;
	InplaceArray<MYSQL_BIND, STATIC_RESULTS> res_;
	InplaceArray<std::unique_ptr<char[]>, 8> buffs_;

	Statement(MYSQL_STMT* stmt, size_t& conn_ref_objs_no)
		: connection_referencing_objects_no_(&conn_ref_objs_no), stmt_(stmt),
			params_(mysql_stmt_param_count(stmt)),
			res_(mysql_stmt_field_count(stmt))
	{
		for (auto& x : params_)
			memset(std::addressof(x), 0, sizeof(x));
		for (auto& x : res_)
			memset(std::addressof(x), 0, sizeof(x));
		bind_res_errors_and_nulls();

		++conn_ref_objs_no;
	}

	Statement(const Statement&) = delete;
	Statement& operator=(const Statement&) = delete;

	void bind_res_errors_and_nulls() noexcept {
		// Bind errors
		for (unsigned i = 0; i < res_.size(); ++i)
			res_[i].error = &res_[i].error_value;
		// Bind nulls
		for (unsigned i = 0; i < res_.size(); ++i)
			res_[i].is_null = &res_[i].is_null_value;
	}

public:
	Statement() : connection_referencing_objects_no_(nullptr), stmt_(nullptr),
		params_(0), res_(0) {}

	Statement(Statement&& s) noexcept
		: connection_referencing_objects_no_(
				std::exchange(s.connection_referencing_objects_no_, nullptr)),
			stmt_(std::exchange(s.stmt_, nullptr)),
			params_(std::move(s.params_)), res_(std::move(s.res_))
	{
		bind_res_errors_and_nulls();
	}

	Statement& operator=(Statement&& s) noexcept {
		if (stmt_)
			(void)mysql_stmt_close(stmt_);
		if (connection_referencing_objects_no_)
			--*connection_referencing_objects_no_;

		connection_referencing_objects_no_ =
			std::exchange(s.connection_referencing_objects_no_, nullptr);
		stmt_ = std::exchange(s.stmt_, nullptr);
		params_ = std::move(s.params_);
		res_ = std::move(s.res_);
		bind_res_errors_and_nulls();

		return *this;
	}

	~Statement() {
		if (stmt_)
			(void)mysql_stmt_close(stmt_);

		if (connection_referencing_objects_no_)
			--*connection_referencing_objects_no_;
	}

	MYSQL_STMT* impl() noexcept { return stmt_; }

	operator MYSQL_STMT* () noexcept { return impl(); }

	// Need to be called before execute()
private:
	void bind(unsigned idx, const bool& x) ND(noexcept) {
		static_assert(sizeof(const bool) == sizeof(const char),
			"Needed below to do the address magic");
		bind(idx, reinterpret_cast<const char&>(x));
	}

public:
	void bind(unsigned idx, const char& x) ND(noexcept) {
		D(throw_assert(idx < params_.size());)
		static_assert(sizeof(char) == sizeof(signed char),
			"Needed to do the pointer conversion");
		params_[idx].buffer_type = MYSQL_TYPE_TINY;
		params_[idx].buffer = const_cast<char*>(&x);
		params_[idx].is_unsigned = false;
	}

	void bind(unsigned idx, const signed char& x) ND(noexcept) {
		D(throw_assert(idx < params_.size());)
		params_[idx].buffer_type = MYSQL_TYPE_TINY;
		params_[idx].buffer = const_cast<signed char*>(&x);
		params_[idx].is_unsigned = false;
	}

	void bind(unsigned idx, const unsigned char& x) ND(noexcept) {
		D(throw_assert(idx < params_.size());)
		params_[idx].buffer_type = MYSQL_TYPE_TINY;
		params_[idx].buffer = const_cast<unsigned char*>(&x);
		params_[idx].is_unsigned = true;
	}

	void bind(unsigned idx, const short& x) ND(noexcept) {
		D(throw_assert(idx < params_.size());)
		params_[idx].buffer_type = MYSQL_TYPE_SHORT;
		params_[idx].buffer = const_cast<short*>(&x);
		params_[idx].is_unsigned = false;
	}

	void bind(unsigned idx, const unsigned short& x) ND(noexcept) {
		D(throw_assert(idx < params_.size());)
		params_[idx].buffer_type = MYSQL_TYPE_SHORT;
		params_[idx].buffer = const_cast<unsigned short*>(&x);
		params_[idx].is_unsigned = true;
	}

	void bind(unsigned idx, const int& x) ND(noexcept) {
		D(throw_assert(idx < params_.size());)
		params_[idx].buffer_type = MYSQL_TYPE_LONG;
		params_[idx].buffer = const_cast<int*>(&x);
		params_[idx].is_unsigned = false;
	}

	void bind(unsigned idx, const unsigned int& x) ND(noexcept) {
		D(throw_assert(idx < params_.size());)
		params_[idx].buffer_type = MYSQL_TYPE_LONG;
		params_[idx].buffer = const_cast<unsigned int*>(&x);
		params_[idx].is_unsigned = true;
	}

	void bind(unsigned idx, const long long& x) ND(noexcept) {
		D(throw_assert(idx < params_.size());)
		params_[idx].buffer_type = MYSQL_TYPE_LONGLONG;
		params_[idx].buffer = const_cast<long long*>(&x);
		params_[idx].is_unsigned = false;
	}

	void bind(unsigned idx, const unsigned long long& x) ND(noexcept) {
		D(throw_assert(idx < params_.size());)
		params_[idx].buffer_type = MYSQL_TYPE_LONGLONG;
		params_[idx].buffer = const_cast<unsigned long long*>(&x);
		params_[idx].is_unsigned = true;
	}

	void bind(unsigned idx, const long& x) ND(noexcept) {
		D(throw_assert(idx < params_.size());)
		params_[idx].buffer_type =
			(sizeof(x) == sizeof(int) ? MYSQL_TYPE_LONG : MYSQL_TYPE_LONGLONG);
		params_[idx].buffer = const_cast<long*>(&x);
		params_[idx].is_unsigned = false;
	}

	void bind(unsigned idx, const unsigned long& x) ND(noexcept) {
		D(throw_assert(idx < params_.size());)
		params_[idx].buffer_type =
			(sizeof(x) == sizeof(int) ? MYSQL_TYPE_LONG : MYSQL_TYPE_LONGLONG);
		params_[idx].buffer = const_cast<unsigned long*>(&x);
		params_[idx].is_unsigned = true;
	}

	void bind(unsigned idx, char* str, size_t& length, size_t max_size)
		ND(noexcept)
	{
		D(throw_assert(idx < params_.size());)
		params_[idx].buffer_type = MYSQL_TYPE_BLOB;
		params_[idx].buffer = str;
		params_[idx].buffer_length = max_size;
		params_[idx].length = &length;
	}

private:
	void bind(unsigned idx, const char* str) ND(noexcept) {
		D(throw_assert(idx < params_.size());)
		auto len = strlen(str);
		params_[idx].buffer_type = MYSQL_TYPE_BLOB;
		params_[idx].buffer = const_cast<char*>(str);
		params_[idx].buffer_length = len;
		params_[idx].length = nullptr;
		params_[idx].length_value = len;
	}

public:
	void bind(unsigned idx, StringView str) ND(noexcept) {
		D(throw_assert(idx < params_.size());)
		params_[idx].buffer_type = MYSQL_TYPE_BLOB;
		params_[idx].buffer = const_cast<char*>(str.data());
		params_[idx].buffer_length = str.size();
		params_[idx].length = nullptr;
		params_[idx].length_value = str.size();
	}

	// Like bind(StringView) but copies string into internal buffer - useful for
	// temporary strings
	void bind_copy(unsigned idx, StringView str) ND(noexcept) {
		D(throw_assert(idx < params_.size());)
		if (str.size()) {
			auto ptr = std::make_unique<char[]>(str.size());
			std::copy(str.begin(), str.end(), ptr.get());
			str = StringView(ptr.get(), str.size());
			buffs_.emplace_back(std::move(ptr));
		}

		bind(idx, str);
	}

	void bind(unsigned idx, const std::string&&) = delete;

	void bind(unsigned idx, std::nullptr_t) ND(noexcept) { null_column(idx); }

	void null_column(unsigned idx) ND(noexcept) {
		D(throw_assert(idx < params_.size());)
		params_[idx].buffer_type = MYSQL_TYPE_NULL;
	}

	void bind_isnull(unsigned idx, my_bool& is_null) ND(noexcept) {
		D(throw_assert(idx < params_.size());)
		params_[idx].is_null = &is_null;
	}

	/// Binds optional value @p x - std::nullopt corresponds to NULL
	template<class T>
	void bind(unsigned idx, MySQL::Optional<T>& x) ND(noexcept) {
		bind(idx, x.value_);
		bind_isnull(idx, x.has_no_value_);
	}

private: // For use only by bindAndExecute()
	/// Binds optional value @p x - std::nullopt corresponds to NULL
	template<class T>
	void bind(unsigned idx, ::Optional<T>& x) ND(noexcept) {
		if (x.has_value())
			bind(idx, *x);
		else
			null_column(idx);
	}

public:
	template<class T>
	void bind(unsigned idx, EnumVal<T>& x) ND(noexcept) {
		bind(idx, static_cast<typename EnumVal<T>::ValType&>(x));
	}

	void fixBinds() {
		if (mysql_stmt_bind_param(stmt_, params_.data()))
			THROW(mysql_stmt_error(stmt_));
	}

	template<class... Args>
	void bind_all(Args&&... args) {
		unsigned idx = 0;
		int t[] = {(bind(idx++, std::forward<Args>(args)), 0)...};
		(void)t;

		fixBinds();
	}

	void execute() {
		if (mysql_stmt_execute(stmt_) or mysql_stmt_store_result(stmt_))
			THROW(mysql_stmt_error(stmt_));
	}

	void fixAndExecute() {
		fixBinds();
		execute();
	}

	template<class... Args>
	void bindAndExecute(Args&&... args) {
		bind_all(args...); // std::forward is omitted intentionally to make
		                   // bind_all() think that all arguments are references
		execute();
	}

	// Need to be called before next()
	void res_bind(unsigned idx, bool& x) ND(noexcept) {
		static_assert(sizeof(bool) == sizeof(char),
			"Needed below to do the address magic");
		res_bind(idx, reinterpret_cast<int8_t&>(x));
	}

	void res_bind(unsigned idx, char& x) ND(noexcept) {
		D(throw_assert(idx < res_.size());)
		static_assert(sizeof(char) == sizeof(signed char),
			"Needed to do the pointer conversion");
		res_[idx].buffer_type = MYSQL_TYPE_TINY;
		res_[idx].buffer = &x;
		res_[idx].is_unsigned = false;
	}

	void res_bind(unsigned idx, signed char& x) ND(noexcept) {
		D(throw_assert(idx < res_.size());)
		res_[idx].buffer_type = MYSQL_TYPE_TINY;
		res_[idx].buffer = &x;
		res_[idx].is_unsigned = false;
	}

	void res_bind(unsigned idx, unsigned char& x) ND(noexcept) {
		D(throw_assert(idx < res_.size());)
		res_[idx].buffer_type = MYSQL_TYPE_TINY;
		res_[idx].buffer = &x;
		res_[idx].is_unsigned = true;
	}

	void res_bind(unsigned idx, short& x) ND(noexcept) {
		D(throw_assert(idx < res_.size());)
		res_[idx].buffer_type = MYSQL_TYPE_SHORT;
		res_[idx].buffer = &x;
		res_[idx].is_unsigned = false;
	}

	void res_bind(unsigned idx, unsigned short& x) ND(noexcept) {
		D(throw_assert(idx < res_.size());)
		res_[idx].buffer_type = MYSQL_TYPE_SHORT;
		res_[idx].buffer = &x;
		res_[idx].is_unsigned = true;
	}

	void res_bind(unsigned idx, int& x) ND(noexcept) {
		D(throw_assert(idx < res_.size());)
		res_[idx].buffer_type = MYSQL_TYPE_LONG;
		res_[idx].buffer = &x;
		res_[idx].is_unsigned = false;
	}

	void res_bind(unsigned idx, unsigned int& x) ND(noexcept) {
		D(throw_assert(idx < res_.size());)
		res_[idx].buffer_type = MYSQL_TYPE_LONG;
		res_[idx].buffer = &x;
		res_[idx].is_unsigned = true;
	}

	void res_bind(unsigned idx, long long& x) ND(noexcept) {
		D(throw_assert(idx < res_.size());)
		res_[idx].buffer_type = MYSQL_TYPE_LONGLONG;
		res_[idx].buffer = &x;
		res_[idx].is_unsigned = false;
	}

	void res_bind(unsigned idx, unsigned long long& x) ND(noexcept) {
		D(throw_assert(idx < res_.size());)
		res_[idx].buffer_type = MYSQL_TYPE_LONGLONG;
		res_[idx].buffer = &x;
		res_[idx].is_unsigned = true;
	}

	void res_bind(unsigned idx, long& x) ND(noexcept) {
		D(throw_assert(idx < res_.size());)
		res_[idx].buffer_type =
			(sizeof(x) == sizeof(int) ? MYSQL_TYPE_LONG : MYSQL_TYPE_LONGLONG);
		res_[idx].buffer = &x;
		res_[idx].is_unsigned = false;
	}

	void res_bind(unsigned idx, unsigned long& x) ND(noexcept) {
		D(throw_assert(idx < res_.size());)
		res_[idx].buffer_type =
			(sizeof(x) == sizeof(int) ? MYSQL_TYPE_LONG : MYSQL_TYPE_LONGLONG);
		res_[idx].buffer = &x;
		res_[idx].is_unsigned = true;
	}

	template<size_t BUFF_SIZE>
	void res_bind(unsigned idx, InplaceBuff<BUFF_SIZE>& buff) ND(noexcept) {
		D(throw_assert(idx < res_.size());)
		static_assert(std::is_base_of<InplaceBuffBase, InplaceBuff<BUFF_SIZE>>
			::value, "Needed by next()");
		res_[idx].buffer_type = MYSQL_TYPE_BLOB;
		res_[idx].buffer = buff.data();
		res_[idx].buffer_length = buff.max_size();
		res_[idx].length = &buff.size;
	}

	void res_bind(unsigned idx, std::nullptr_t) ND(noexcept) {
		res_null_column(idx);
	}

	void res_null_column(unsigned idx) ND(noexcept) {
		D(throw_assert(idx < res_.size());)
		res_[idx].buffer_type = MYSQL_TYPE_NULL;
	}

	void res_bind_isnull(unsigned idx, my_bool& is_null) ND(noexcept) {
		D(throw_assert(idx < res_.size());)
		res_[idx].is_null = &is_null;
	}

	/// Binds optional value @p x - std::nullopt corresponds to NULL
	template<class T>
	void res_bind(unsigned idx, MySQL::Optional<T>& x) ND(noexcept) {
		res_bind(idx, x.value_);
		res_bind_isnull(idx, x.has_no_value_);
	}

	template<class T>
	void res_bind(unsigned idx, EnumVal<T>& x) ND(noexcept) {
		res_bind(idx, static_cast<typename EnumVal<T>::ValType&>(x));
	}

	void resFixBinds() {
		if (mysql_stmt_bind_result(stmt_, res_.data()))
			THROW(mysql_stmt_error(stmt_));
	}

	template<class... Args>
	void res_bind_all(Args&&... args) {
		unsigned idx = 0;
		int t[] = {(res_bind(idx++, std::forward<Args>(args)), 0)...};
		(void)t;

		resFixBinds();
	}

	bool is_null(unsigned idx) ND(noexcept) {
		D(throw_assert(idx < res_.size());)
		return *res_[idx].is_null;
	}

	bool next() {
		int rc = mysql_stmt_fetch(stmt_);
		if (rc == 0)
			return true;
		if (rc == MYSQL_NO_DATA) {
			mysql_stmt_free_result(stmt_);
			return false;
		}

		if (rc != MYSQL_DATA_TRUNCATED)
			THROW(mysql_stmt_error(stmt_));

		// Truncation occurred
		for (unsigned i = 0; i < res_.size(); ++i) {
			if (not *res_[i].error)
				continue;

			if (res_[i].buffer_type != MYSQL_TYPE_BLOB)
				THROW("Truncated data at column ", i);

			InplaceBuffBase* buff = reinterpret_cast<InplaceBuffBase*>
				((int8_t*)res_[i].length - offsetof(InplaceBuffBase, size));
			buff->lossy_resize(buff->size);

			res_[i].buffer = buff->data();
			res_[i].buffer_length = buff->max_size();

			if (mysql_stmt_fetch_column(stmt_, &res_[i], i, 0))
				THROW(mysql_stmt_error(stmt_));
		}

		resFixBinds();
		return true;
	}

	unsigned fields_num() noexcept { return mysql_stmt_field_count(stmt_); }

	my_ulonglong rows_num() noexcept { return mysql_stmt_num_rows(stmt_); }

	my_ulonglong insert_id() noexcept { return mysql_stmt_insert_id(stmt_); }

	my_ulonglong affected_rows() noexcept {
		return mysql_stmt_affected_rows(stmt_);
	}

	std::unique_ptr<MYSQL_RES, decltype(&mysql_free_result)> resMetadata()
		noexcept
	{
		return {mysql_stmt_result_metadata(stmt_), &mysql_free_result};
	}
};

class Transaction {
	friend class Connection;

	Connection* conn_;
	std::function<void()> rollback_action_;

	Transaction(Connection& conn);

public:
	Transaction() : conn_(nullptr) {}

	Transaction(const Transaction&) = delete;
	Transaction& operator=(const Transaction&) = delete;

	Transaction(Transaction&& trans) noexcept
		: conn_(std::exchange(trans.conn_, nullptr)) {}

	Transaction& operator=(Transaction&& trans) {
		rollback();
		conn_ = std::exchange(trans.conn_, nullptr);
		return *this;
	}

	void rollback();

	// Sets a callback that will be called AFTER transaction rollback
	void set_rollback_action(std::function<void()> rollback_action) {
		rollback_action_ = std::move(rollback_action);
	}

	void commit();

	~Transaction();
};

class Connection {
private:
	struct Library {
		Library() {
			// Must be done before starting threads
			if (mysql_library_init(0, nullptr, nullptr))
				THROW("Could not initialize MySQL library");
		}

		~Library() { mysql_library_end(); }
	};
	static const Library init_;

	MYSQL* conn_;
	size_t referencing_objects_no_ = 0;

	Connection(const Connection&) = delete;
	Connection& operator=(const Connection&) = delete;

	friend class Transaction;

DEBUG_MYSQL(
	static size_t get_next_connection_id() noexcept {
		static std::atomic_size_t next_connection_id = 0;
		return next_connection_id++;
	}

	size_t connection_id = get_next_connection_id();
)

	void reconnect() {
		InplaceBuff<32> host(conn_->host), user(conn_->user),
			passwd(conn_->passwd), db(conn_->db);

		Connection c;
		c.connect(host, user, passwd, db);
		*this = std::move(c);
	}

	template<class T>
	static bool ret_val_success(T* ret) noexcept { return (ret != nullptr); }

	template<class T>
	static std::enable_if_t<std::is_integral<T>::value, bool>
	ret_val_success(T ret) noexcept { return ret == T(); }

	template<class Func2, class Func3, class Func, class... Args>
	auto call_and_try_reconnecting_on_error_impl(Func2&& resetter,
		Func3&& error_msg, Func&& func, Args&&... args)
	{
		STACK_UNWINDING_MARK;
		// std::forward is omitted as we may use arguments second time
		auto rc = func(args...);
		if (ret_val_success(rc))
			return rc;

		if (not isOneOf((int)mysql_errno(conn_), CR_SERVER_GONE_ERROR, CR_SERVER_LOST))
			THROW(error_msg());

		// Cannot reconnect if other objects already uses this connection
		if (referencing_objects_no_ != 0)
			THROW(error_msg());

		reconnect();
		resetter();

		rc = func(args...);
		if (ret_val_success(rc))
			return rc;

		THROW(error_msg());
	}

	template<class Func, class... Args>
	auto call_and_try_reconnecting_on_error(Func&& func, Args&&... args) {
		STACK_UNWINDING_MARK;
		return call_and_try_reconnecting_on_error_impl([]{},
			[&] { return mysql_error(conn_); },
			std::forward<Func>(func), std::forward<Args>(args)...);
	}

public:
	Connection() {
		static std::mutex mysql_protector;
		mysql_protector.lock();
		conn_ = mysql_init(nullptr);
		mysql_protector.unlock();
		if (not conn_)
			THROW("mysql_init() failed - not enough memory is available");
	}

	Connection(Connection&& c) : conn_(std::exchange(c.conn_, nullptr)),
		referencing_objects_no_(c.referencing_objects_no_)
		DEBUG_MYSQL(, connection_id(c.connection_id)) {}

	Connection& operator=(Connection&& c) {
		close();
		conn_ = std::exchange(c.conn_, nullptr);
		referencing_objects_no_ = c.referencing_objects_no_;
		DEBUG_MYSQL(connection_id = c.connection_id);
		return *this;
	}

	void close() noexcept {
		if (conn_) {
			mysql_close(conn_);
			conn_ = nullptr;
		}
	}

	~Connection() {
		if (conn_)
			mysql_close(conn_);
	}

	MYSQL* impl() noexcept { return conn_; }

	operator MYSQL* () noexcept { return impl(); }

	void connect(FilePath host, FilePath user, FilePath passwd, FilePath db) {
		STACK_UNWINDING_MARK;

		my_bool x = true;
		(void)mysql_options(conn_, MYSQL_REPORT_DATA_TRUNCATION, &x);

		if (not mysql_real_connect(conn_, host, user, passwd, db, 0, nullptr, 0))
			THROW(mysql_error(conn_));

		// Set CLOEXEC flag on the mysql socket
		int mysql_socket = mysql_get_socket(conn_);
		int flags = fcntl(mysql_socket, F_GETFD);
		if (flags == -1)
			THROW("fcntl()", errmsg());
		if (fcntl(mysql_socket, F_SETFD, flags | FD_CLOEXEC) == -1)
			THROW("fcntl()", errmsg());
	}

	bool disconnected() noexcept {
		return (mysql_ping(conn_) != 0 and mysql_errno(conn_) == CR_SERVER_GONE_ERROR);
	}

	Result query(StringView sql) {
		STACK_UNWINDING_MARK;
		DEBUG_MYSQL(errlog("MySQL (connection ", connection_id, "): query -> ", sql);)
		call_and_try_reconnecting_on_error(mysql_real_query, *this,
			sql.data(), sql.size());

		MYSQL_RES* res = call_and_try_reconnecting_on_error(mysql_store_result,
			*this);
		return {res, referencing_objects_no_};
	}

	void update(StringView sql) {
		STACK_UNWINDING_MARK;
		DEBUG_MYSQL(errlog("MySQL (connection ", connection_id, "): update -> ", sql);)
		call_and_try_reconnecting_on_error(mysql_real_query, *this,
			sql.data(), sql.size());
	}

	template<size_t STATIC_BINDS = 16, size_t STATIC_RESULTS = 16>
	Statement<STATIC_BINDS, STATIC_RESULTS> prepare(StringView sql) {
		STACK_UNWINDING_MARK;
		DEBUG_MYSQL(errlog("MySQL (connection ", connection_id, "): prepare -> ", sql);)
		MYSQL_STMT* stmt = call_and_try_reconnecting_on_error(mysql_stmt_init, *this);
		try {
			call_and_try_reconnecting_on_error_impl([&]{
				(void)mysql_stmt_close(stmt);
				stmt = call_and_try_reconnecting_on_error(mysql_stmt_init, *this);
			}, [&]{ return mysql_stmt_error(stmt); },
			mysql_stmt_prepare, stmt, sql.data(), sql.size());

		} catch (...) {
			(void)mysql_stmt_close(stmt);
			throw;
		}

		return {stmt, referencing_objects_no_};
	}

	my_ulonglong insert_id() noexcept { return mysql_insert_id(conn_); }

	Transaction start_transaction() { return Transaction(*this); }

	Transaction start_serializable_transaction() {
		STACK_UNWINDING_MARK;
		update("SET TRANSACTION ISOLATION LEVEL SERIALIZABLE");
		return Transaction(*this);
	}

	Transaction start_read_committed_transaction() {
		STACK_UNWINDING_MARK;
		update("SET TRANSACTION ISOLATION LEVEL READ COMMITTED");
		return Transaction(*this);
	}
};

inline Transaction::Transaction(Connection& conn) : conn_(&conn) {
	conn_->update("START TRANSACTION");
	// This have to be done last because of potential exceptions
	++conn_->referencing_objects_no_;
}

inline void Transaction::rollback() {
	if (conn_) {
		if (mysql_rollback(*conn_))
			THROW(mysql_error(*conn_));

		--conn_->referencing_objects_no_;
		conn_ = nullptr;

		if (rollback_action_)
			rollback_action_();
	}
}

inline void Transaction::commit() {
	if (mysql_commit(*conn_)) {
		THROW(mysql_error(*conn_));
	}

	--conn_->referencing_objects_no_;
	conn_ = nullptr;
}

inline Transaction::~Transaction() {
	try { rollback(); } catch (...) {} // There is nothing we can do with exceptions
	// If the exception was thrown the counter may have not been decremented
	if (conn_)
		--conn_->referencing_objects_no_;
}

} // namespace MySQL

#endif // __has_include(<mysql.h>)

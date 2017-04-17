#pragma once

#include "debug.h"
#include "string.h"
#include "utilities.h"

#include <cstddef>

#if __has_include(<mysql/mysql.h>)
#include <mysql/mysql.h>

namespace MySQL {

class Connection;

class Result {
private:
	friend Connection;

	MYSQL_RES* res_;
	MYSQL_ROW row_ {};
	unsigned long* lengths_ {};

	Result(MYSQL_RES* res) : res_{res} {}

	Result(const Result&) = delete;
	Result& operator=(const Result&) = delete;
	Result& operator=(Result&&) = delete;

public:
	Result(Result&& r) noexcept : res_ {r.res_}, row_ {r.row_} {
		r.res_ = nullptr;
		r.row_ = {};
	}

	~Result() { mysql_free_result(res_); }

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

	bool isNull(unsigned i) noexcept { return (row_[i] == nullptr); }

	StringView operator[](unsigned i) noexcept { return {row_[i], lengths_[i]}; }
};

template<size_t STATIC_BINDS = 16, size_t STATIC_RESULTS = 16>
class Statement {
private:
	friend Connection;

	MYSQL_STMT* stmt_;
	InplaceArray<MYSQL_BIND, STATIC_BINDS> params_;
	InplaceArray<MYSQL_BIND, STATIC_RESULTS> res_;

	Statement(MYSQL_STMT* stmt)
		: stmt_ {stmt}, params_ {mysql_stmt_param_count(stmt)},
		res_ {mysql_stmt_field_count(stmt)}
	{
		memset(params_.data(), 0, sizeof(params_[0]) * params_.size());
		memset(res_.data(), 0, sizeof(res_[0]) * res_.size());

		// Bind errors
		for (unsigned i = 0; i < res_.size(); ++i)
			res_[i].error = &res_[i].error_value;
	}

	Statement(const Statement&) = delete;
	Statement& operator=(const Statement&) = delete;

public:
	Statement() : stmt_ {nullptr}, params_ {0}, res_ {0} {}

	Statement(Statement&& s) noexcept : stmt_ {s.stmt_},
		params_ {std::move(s.params_)}, res_ {std::move(s.res_)}
	{
		s.stmt_ = nullptr;
		// Bind errors
		for (unsigned i = 0; i < res_.size(); ++i)
			res_[i].error = &res_[i].error_value;
	}

	Statement& operator=(Statement&& s) {
		if (stmt_)
			mysql_stmt_close(stmt_);

		stmt_ = s.stmt_;
		s.stmt_ = nullptr;
		params_ = std::move(s.params_);
		res_ = std::move(s.res_);

		// Bind errors
		for (unsigned i = 0; i < res_.size(); ++i)
			res_[i].error = &res_[i].error_value;

		return *this;
	}

	~Statement() {
		if (stmt_)
			mysql_stmt_close(stmt_);
	}

	MYSQL_STMT* impl() noexcept { return stmt_; }

	operator MYSQL_STMT* () noexcept { return impl(); }

	// Need to be called before execute()
	void bind(unsigned idx, char& x) noexcept {
		params_[idx].buffer_type = MYSQL_TYPE_TINY;
		params_[idx].buffer = &x;
		params_[idx].is_unsigned = false;
	}

	void bind(unsigned idx, unsigned char& x) noexcept {
		params_[idx].buffer_type = MYSQL_TYPE_TINY;
		params_[idx].buffer = &x;
		params_[idx].is_unsigned = true;
	}

	void bind(unsigned idx, short& x) noexcept {
		params_[idx].buffer_type = MYSQL_TYPE_SHORT;
		params_[idx].buffer = &x;
		params_[idx].is_unsigned = false;
	}

	void bind(unsigned idx, unsigned short& x) noexcept {
		params_[idx].buffer_type = MYSQL_TYPE_SHORT;
		params_[idx].buffer = &x;
		params_[idx].is_unsigned = true;
	}

	void bind(unsigned idx, int& x) noexcept {
		params_[idx].buffer_type = MYSQL_TYPE_LONG;
		params_[idx].buffer = &x;
		params_[idx].is_unsigned = false;
	}

	void bind(unsigned idx, unsigned int& x) noexcept {
		params_[idx].buffer_type = MYSQL_TYPE_LONG;
		params_[idx].buffer = &x;
		params_[idx].is_unsigned = true;
	}

	void bind(unsigned idx, long long& x) noexcept {
		params_[idx].buffer_type = MYSQL_TYPE_LONGLONG;
		params_[idx].buffer = &x;
		params_[idx].is_unsigned = false;
	}

	void bind(unsigned idx, unsigned long long& x) noexcept {
		params_[idx].buffer_type = MYSQL_TYPE_LONGLONG;
		params_[idx].buffer = &x;
		params_[idx].is_unsigned = true;
	}

	void bind(unsigned idx, long& x) noexcept {
		params_[idx].buffer_type =
			(sizeof(x) == sizeof(int) ? MYSQL_TYPE_LONG : MYSQL_TYPE_LONGLONG);
		params_[idx].buffer = &x;
		params_[idx].is_unsigned = false;
	}

	void bind(unsigned idx, unsigned long& x) noexcept {
		params_[idx].buffer_type =
			(sizeof(x) == sizeof(int) ? MYSQL_TYPE_LONG : MYSQL_TYPE_LONGLONG);
		params_[idx].buffer = &x;
		params_[idx].is_unsigned = true;
	}

	void bind(unsigned idx, char* str, size_t& length, size_t max_size) noexcept
	{
		params_[idx].buffer_type = MYSQL_TYPE_BLOB;
		params_[idx].buffer = str;
		params_[idx].buffer_length = max_size;
		params_[idx].length = &length;
	}

	void bind(unsigned idx, StringView str) noexcept {
		params_[idx].buffer_type = MYSQL_TYPE_BLOB;
		params_[idx].buffer = const_cast<char*>(str.data());
		params_[idx].buffer_length = str.size();
		params_[idx].length = nullptr;
		params_[idx].length_value = str.size();
	}

	void bind(unsigned idx, std::string&&) = delete;

	template<size_t BUFF_SIZE>
	void bind(unsigned idx, StringBuff<BUFF_SIZE>& buff) noexcept {
		bind(idx, buff.str, buff.len, buff.max_size);
	}

	void bind(unsigned idx, std::nullptr_t) noexcept { null_column(idx); }

	void null_column(unsigned idx) noexcept {
		params_[idx].buffer_type = MYSQL_TYPE_NULL;
	}

	void bind_isnull(unsigned idx, my_bool& is_null) noexcept {
		params_[idx].is_null = &is_null;
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
	void res_bind(unsigned idx, char& x) noexcept {
		res_[idx].buffer_type = MYSQL_TYPE_TINY;
		res_[idx].buffer = &x;
		res_[idx].is_unsigned = false;
	}

	void res_bind(unsigned idx, unsigned char& x) noexcept {
		res_[idx].buffer_type = MYSQL_TYPE_TINY;
		res_[idx].buffer = &x;
		res_[idx].is_unsigned = true;
	}

	void res_bind(unsigned idx, short& x) noexcept {
		res_[idx].buffer_type = MYSQL_TYPE_SHORT;
		res_[idx].buffer = &x;
		res_[idx].is_unsigned = false;
	}

	void res_bind(unsigned idx, unsigned short& x) noexcept {
		res_[idx].buffer_type = MYSQL_TYPE_SHORT;
		res_[idx].buffer = &x;
		res_[idx].is_unsigned = true;
	}

	void res_bind(unsigned idx, int& x) noexcept {
		res_[idx].buffer_type = MYSQL_TYPE_LONG;
		res_[idx].buffer = &x;
		res_[idx].is_unsigned = false;
	}

	void res_bind(unsigned idx, unsigned int& x) noexcept {
		res_[idx].buffer_type = MYSQL_TYPE_LONG;
		res_[idx].buffer = &x;
		res_[idx].is_unsigned = true;
	}

	void res_bind(unsigned idx, long long& x) noexcept {
		res_[idx].buffer_type = MYSQL_TYPE_LONGLONG;
		res_[idx].buffer = &x;
		res_[idx].is_unsigned = false;
	}

	void res_bind(unsigned idx, unsigned long long& x) noexcept {
		res_[idx].buffer_type = MYSQL_TYPE_LONGLONG;
		res_[idx].buffer = &x;
		res_[idx].is_unsigned = true;
	}

	void res_bind(unsigned idx, long& x) noexcept {
		res_[idx].buffer_type =
			(sizeof(x) == sizeof(int) ? MYSQL_TYPE_LONG : MYSQL_TYPE_LONGLONG);
		res_[idx].buffer = &x;
		res_[idx].is_unsigned = false;
	}

	void res_bind(unsigned idx, unsigned long& x) noexcept {
		res_[idx].buffer_type =
			(sizeof(x) == sizeof(int) ? MYSQL_TYPE_LONG : MYSQL_TYPE_LONGLONG);
		res_[idx].buffer = &x;
		res_[idx].is_unsigned = true;
	}

	template<size_t BUFF_SIZE>
	void res_bind(unsigned idx, InplaceBuff<BUFF_SIZE>& buff) noexcept {
		static_assert(std::is_base_of<InplaceBuffBase, InplaceBuff<BUFF_SIZE>>
			::value, "Needed by next()");
		res_[idx].buffer_type = MYSQL_TYPE_BLOB;
		res_[idx].buffer = buff.data();
		res_[idx].buffer_length = buff.max_size();
		res_[idx].length = &buff.size;
	}

	void res_bind(unsigned idx, std::nullptr_t) noexcept {
		res_null_column(idx);
	}

	void res_null_column(unsigned idx) noexcept {
		res_[idx].buffer_type = MYSQL_TYPE_NULL;
	}

	void res_bind_isnull(unsigned idx, my_bool& is_null) noexcept {
		res_[idx].is_null = &is_null;
	}

	void resFixBinds() {
		if (mysql_stmt_bind_result(stmt_, res_.data()))
			THROW(mysql_stmt_error(stmt_));
	}

	template<class... Args>
	void res_bind_all(Args&... args) {
		unsigned idx = 0;
		int t[] = {(res_bind(idx++, args), 0)...}; // Forward is omitted because
		                                           // args are references
		(void)t;

		resFixBinds();
	}

	bool isNull(unsigned i) noexcept {
		// TODO: check if works when is_null == nullptr
		return res_[i].is_null ? *res_[i].is_null : res_[i].is_null_value;
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
				THROW("Truncated data at column ", toStr(i));

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

	MYSQL_RES* resMetadata() noexcept {
		return mysql_stmt_result_metadata(stmt_);
	}
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

	MYSQL conn_;

	Connection(const Connection&) = delete;
	Connection& operator=(const Connection&) = delete;

public:
	Connection() {
		if (not mysql_init(&conn_))
			THROW(mysql_error(&conn_));
	}

	Connection(Connection&& c) noexcept {
		memcpy(&conn_, &c.conn_, sizeof(conn_));
		c.conn_ = {};
	}

	Connection& operator=(Connection&& c) {
		close();

		memcpy(&conn_, &c.conn_, sizeof(conn_));
		c.conn_ = {};

		return *this;
	}

	MYSQL* impl() noexcept { return &conn_; }

	operator MYSQL* () noexcept { return impl(); }

	void connect(CStringView host, CStringView user, CStringView passwd,
		CStringView db)
	{
		my_bool x = true;
		mysql_options(&conn_, MYSQL_OPT_RECONNECT, &x);
		mysql_options(&conn_, MYSQL_REPORT_DATA_TRUNCATION, &x);

		if (not mysql_real_connect(&conn_, host.data(), user.data(),
			passwd.data(), db.data(), 0, nullptr, 0))
		{
			THROW(mysql_error(&conn_));
		}
	}

	Result query(StringView sql) {
		if (mysql_real_query(&conn_, sql.data(), sql.size()))
			THROW(mysql_error(&conn_));

		MYSQL_RES* res = mysql_use_result(&conn_);
		if (not res)
			THROW(mysql_error(&conn_));

		return {res};
	}

	void update(StringView sql) {
		if (mysql_real_query(&conn_, sql.data(), sql.size()))
			THROW(mysql_error(&conn_));
	}

	template<size_t STATIC_BINDS = 16, size_t STATIC_RESULTS = 16>
	Statement<STATIC_BINDS, STATIC_RESULTS> prepare(StringView sql) {
		MYSQL_STMT* stmt = mysql_stmt_init(&conn_);
		if (not stmt || mysql_stmt_prepare(stmt, sql.data(), sql.size()))
			THROW(mysql_error(&conn_));

		return {stmt};
	}

	my_ulonglong insert_id() noexcept { return mysql_insert_id(&conn_); }

	void close() noexcept { mysql_close(&conn_); }

	~Connection() { close(); }
};

} // namespace MySQL

#endif // __has_include(<mysql/mysql.h>)

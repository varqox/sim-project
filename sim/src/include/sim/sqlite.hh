#pragma once

#include <simlib/debug.hh>
#include <simlib/string.hh>
#include <sqlite3.h>

#define THROW_SQLITE_ERROR(db, ...)                                            \
	THROW(__VA_ARGS__, " - ", sqlite3_errcode(db), ": ", sqlite3_errmsg(db))

namespace SQLite {

class Statement {
	sqlite3_stmt* stmt = nullptr;

	explicit Statement(sqlite3_stmt* st) noexcept : stmt(st) {}

public:
	Statement() noexcept = default;

	Statement(const Statement&) = delete;

	Statement(Statement&& s) : stmt {s.stmt} { s.stmt = nullptr; }

	Statement& operator=(const Statement&) = delete;

	Statement& operator=(Statement&& s) {
		if (stmt)
			sqlite3_finalize(stmt);

		stmt = s.stmt;
		s.stmt = nullptr;
		return *this;
	}

	operator sqlite3_stmt*() noexcept { return stmt; }

	~Statement() { sqlite3_finalize(stmt); }

	int prepare(sqlite3* db, CStringView sql) {
		return sqlite3_prepare_v2(db, sql.data(), sql.size() + 1, &stmt,
		                          nullptr);
	}

	int step() {
		int rc = sqlite3_step(stmt);
		if (rc == SQLITE_DONE || rc == SQLITE_ROW)
			return rc;

		THROW_SQLITE_ERROR(sqlite3_db_handle(stmt), "sqlite3_step()");
	}

	int step_nothrow() noexcept { return sqlite3_step(stmt); }

	int reset() noexcept { return sqlite3_reset(stmt); }

	int clear_bindings() noexcept { return sqlite3_clear_bindings(stmt); }

	// TODO: this is buggy, no time now to fix it
	// int finalize() noexcept {
	// 	int rc = sqlite3_reset(stmt);
	// 	stmt = nullptr;
	// 	return rc;
	// }

	// Bind data (throwing)
	void bind_null(int i_col) {
		if (sqlite3_bind_null(stmt, i_col))
			THROW_SQLITE_ERROR(sqlite3_db_handle(stmt), "sqlite3_bind_null()");
	}

	void bind_text(int i_col, StringView val, void (*func)(void*)) {
		if (sqlite3_bind_text(stmt, i_col, val.data(), val.size(), func))
			THROW_SQLITE_ERROR(sqlite3_db_handle(stmt), "sqlite3_bind_text()");
	}

	void bind_int(int i_col, int val) {
		if (sqlite3_bind_int(stmt, i_col, val))
			THROW_SQLITE_ERROR(sqlite3_db_handle(stmt), "sqlite3_bind_int()");
	}

	void bind_int64(int i_col, sqlite3_int64 val) {
		if (sqlite3_bind_int64(stmt, i_col, val))
			THROW_SQLITE_ERROR(sqlite3_db_handle(stmt), "sqlite3_bind_int64()");
	}

	void bind_double(int i_col, double val) {
		if (sqlite3_bind_double(stmt, i_col, val))
			THROW_SQLITE_ERROR(sqlite3_db_handle(stmt),
			                   "sqlite3_bind_double()");
	}

	// Bind data (nothrow)
	int bind_null_nothrow(int i_col) noexcept {
		return sqlite3_bind_null(stmt, i_col);
	}

	int bind_text_nothrow(int i_col, StringView val,
	                     void (*func)(void*)) noexcept {
		return sqlite3_bind_text(stmt, i_col, val.data(), val.size(), func);
	}

	int bind_int_nothrow(int i_col, int val) noexcept {
		return sqlite3_bind_int(stmt, i_col, val);
	}

	int bind_int64_nothrow(int i_col, sqlite3_int64 val) noexcept {
		return sqlite3_bind_int64(stmt, i_col, val);
	}

	int bind_double_nothrow(int i_col, double val) noexcept {
		return sqlite3_bind_double(stmt, i_col, val);
	}

	// Retrieve data
	const char* get_c_str(int i_col) noexcept {
		return (const char*)sqlite3_column_text(stmt, i_col);
	}

	CStringView get_str(int i_col) noexcept {
		return CStringView((const char*)sqlite3_column_text(stmt, i_col),
		                   sqlite3_column_bytes(stmt, i_col));
	}

	int get_int(int i_col) noexcept { return sqlite3_column_int(stmt, i_col); }

	auto get_int64(int i_col) noexcept {
		return sqlite3_column_int64(stmt, i_col);
	}

	double get_double(int i_col) noexcept {
		return sqlite3_column_double(stmt, i_col);
	}

	friend class Connection;
};

class Connection {
	sqlite3* db = nullptr;

public:
	Connection() noexcept = default;

	Connection(FilePath file, int flags, const char* z_vfs = nullptr) {
		if (sqlite3_open_v2(file, &db, flags, z_vfs))
			THROW_SQLITE_ERROR(db, "sqlite3_open_v2()");
	}

	Connection(const Connection&) = delete;

	Connection(Connection&& c) noexcept : db {c.db} { c.db = nullptr; }

	Connection& operator=(const Connection&) = delete;

	Connection& operator=(Connection&& c) noexcept {
		if (db)
			sqlite3_close(db);

		db = c.db;
		c.db = nullptr;
		return *this;
	}

	operator sqlite3*() noexcept { return db; }

	~Connection() { sqlite3_close(db); }

	void close() noexcept {
		sqlite3_close(db);
		db = nullptr;
	}

	void execute(CStringView sql) {
		if (sqlite3_exec(db, sql.data(), nullptr, nullptr, nullptr))
			THROW_SQLITE_ERROR(db, "sqlite3_exec()");
	}

	Statement prepare(CStringView sql) {
		sqlite3_stmt* stmt;
		if (sqlite3_prepare_v2(db, sql.data(), sql.size() + 1, &stmt,
		                       nullptr)) {
			THROW_SQLITE_ERROR(db, "sqlite3_prepare_v2()");
		}
		return Statement {stmt};
	}
};

} // namespace SQLite

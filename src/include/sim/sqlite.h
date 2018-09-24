#pragma once

#include <simlib/debug.h>
#include <simlib/string.h>
#include <sqlite3.h>

#define THROW_SQLITE_ERROR(db, ...) THROW(__VA_ARGS__, " - ", \
	sqlite3_errcode(db), ": ", sqlite3_errmsg(db))

namespace SQLite {

class Statement {
	sqlite3_stmt *stmt = nullptr;

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

	int prepare(sqlite3* db, CStringView zSql) {
		return sqlite3_prepare_v2(db, zSql.data(), zSql.size() + 1, &stmt,
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

	int clearBindings() noexcept { return sqlite3_clear_bindings(stmt); }

	// TODO: this is buggy, no time now to fix it
	// int finalize() noexcept {
	// 	int rc = sqlite3_reset(stmt);
	// 	stmt = nullptr;
	// 	return rc;
	// }

	// Bind data (throwing)
	void bindNull(int iCol) {
		if (sqlite3_bind_null(stmt, iCol))
			THROW_SQLITE_ERROR(sqlite3_db_handle(stmt), "sqlite3_bind_null()");
	}

	void bindText(int iCol, StringView val, void(*func)(void*)) {
		if (sqlite3_bind_text(stmt, iCol, val.data(), val.size(), func))
			THROW_SQLITE_ERROR(sqlite3_db_handle(stmt), "sqlite3_bind_text()");
	}

	void bindInt(int iCol, int val) {
		if (sqlite3_bind_int(stmt, iCol, val))
			THROW_SQLITE_ERROR(sqlite3_db_handle(stmt), "sqlite3_bind_int()");
	}

	void bindInt64(int iCol, sqlite3_int64 val) {
		if (sqlite3_bind_int64(stmt, iCol, val))
			THROW_SQLITE_ERROR(sqlite3_db_handle(stmt), "sqlite3_bind_int64()");
	}

	void bindDouble(int iCol, double val) {
		if (sqlite3_bind_double(stmt, iCol, val))
			THROW_SQLITE_ERROR(sqlite3_db_handle(stmt),
				"sqlite3_bind_double()");
	}

	// Bind data (nothrow)
	int bindNull_nothrow(int iCol) noexcept {
		return sqlite3_bind_null(stmt, iCol);
	}

	int bindText_nothrow(int iCol, StringView val, void(*func)(void*)) noexcept
	{
		return sqlite3_bind_text(stmt, iCol, val.data(), val.size(), func);
	}

	int bindInt_nothrow(int iCol, int val) noexcept {
		return sqlite3_bind_int(stmt, iCol, val);
	}

	int bindInt64_nothrow(int iCol, sqlite3_int64 val) noexcept {
		return sqlite3_bind_int64(stmt, iCol, val);
	}

	int bindDouble_nothrow(int iCol, double val) noexcept {
		return sqlite3_bind_double(stmt, iCol, val);
	}

	// Retrieve data
	const char* getCStr(int iCol) noexcept {
		return (const char*)sqlite3_column_text(stmt, iCol);
	}

	CStringView getStr(int iCol) noexcept {
		return CStringView((const char*)sqlite3_column_text(stmt, iCol), sqlite3_column_bytes(stmt, iCol));
	}

	int getInt(int iCol) noexcept { return sqlite3_column_int(stmt, iCol); }

	auto getInt64(int iCol) noexcept {
		return sqlite3_column_int64(stmt, iCol);
	}

	double getDouble(int iCol) noexcept {
		return sqlite3_column_double(stmt, iCol);
	}


	friend class Connection;
};

class Connection {
	sqlite3 *db = nullptr;

public:
	Connection() noexcept = default;

	Connection(CStringView file, int flags, const char *zVfs = nullptr) {
		if (sqlite3_open_v2(file.c_str(), &db, flags, zVfs))
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

	void execute(CStringView zSql) {
		if (sqlite3_exec(db, zSql.data(), nullptr, nullptr, nullptr))
			THROW_SQLITE_ERROR(db, "sqlite3_exec()");
	}

	Statement prepare(CStringView zSql) {
		sqlite3_stmt *stmt;
		if (sqlite3_prepare_v2(db, zSql.data(), zSql.size() + 1, &stmt,
			nullptr))
		{
			THROW_SQLITE_ERROR(db, "sqlite3_prepare_v2()");
		}
		return Statement {stmt};
	}
};

} // namespace SQLite

#pragma once

#include "memory.h"

#include <mysql_connection.h>

namespace DB {

class Connection {
private:
	UniquePtr<sql::Connection> conn_;
	std::string host_, user_, password_, database_;

	Connection(const Connection&);
	Connection& operator=(const Connection&);

	void connect();

public:
	Connection(const std::string& host, const std::string& user,
			const std::string& password, const std::string& database);

	~Connection() {}

	sql::Connection* mysql() {
		if (conn_->isClosed())
			connect();
		return conn_.get();
	}

	sql::Connection& operator*() { return *mysql(); }

	sql::Connection* operator->() { return mysql(); }
};

/**
 * @brief Creates Connection using file @p filename
 * @details File format: "USER\nPASSWORD\nDATABASE\nHOST"
 *
 * @param filename file to load credentials from
 * @return valid pointer (on error throws std::runtime_error)
 */
Connection* createConnectionUsingPassFile(const char* filename) throw();

/**
 * @brief Transaction wrapper
 */
class Transaction {
	static const int AUTOCOMMIT = 1;
	static const int COMMIT = 2;
	char flag;
	Connection& conn;

public:
	/**
	 * @brief Construct transaction
	 *
	 * @param c connection to use
	 * @param to_commit whether commit at destruction or not
	 */
	Transaction(Connection& c, bool to_commit = false)
		: flag((c.mysql()->getAutoCommit() ? AUTOCOMMIT : 0) |
			(to_commit ? COMMIT : 0)), conn(c) {
		c.mysql()->setAutoCommit(false);
	}

	// Set transaction to commit at destruction
	void toCommit() { flag |= COMMIT; }

	// Set transaction to rollback at destruction
	void toRollback() { flag &= ~COMMIT; }

	~Transaction() {
		if (flag & COMMIT)
			conn.mysql()->commit();
		else
			conn.mysql()->rollback();

		conn.mysql()->setAutoCommit(flag & AUTOCOMMIT);
	}
};

} // namespace DB

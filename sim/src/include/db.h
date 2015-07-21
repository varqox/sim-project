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

} // namespace DB

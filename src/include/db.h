#pragma once

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <mysql_connection.h>

namespace DB {

class Connection {
private:
	sql::Connection *conn_;
	std::string host_, user_, password_, database_;

	Connection(const Connection&);
	Connection& operator=(const Connection&);

	void connect();

public:
	Connection(const std::string& host, const std::string& user,
			const std::string& password, const std::string& database);
	~Connection() { delete conn_; }

	sql::Connection* mysql() {
		if (conn_->isClosed())
			connect();
		return conn_;
	}

	sql::Connection& operator*() { return *mysql(); }

	sql::Connection* operator->() { return mysql(); }
};

} // namespace DB

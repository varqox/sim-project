#pragma once

#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

class DB
{
private:
	sql::Connection * con;
	char *user, *password, *database;

	DB(const DB&);

	DB& operator=(const DB&);

	DB();

	void connect();

	~DB()
	{
		delete con;
		free(user);
		free(password);
		free(database);
	}

	static DB obj;

public:
	static sql::Connection* mysql()
	{
		if(obj.con->isClosed())
			obj.connect();
		return obj.con;
	}
};

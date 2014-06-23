#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

class DB
{
private:
	sql::Connection * con;
	DB(): con(get_driver_instance()->connect("localhost", "sim", "net117working"))
	{con->setSchema("sim");}

	~DB()
	{delete con;}

	static DB obj;

public:
	static sql::Connection* mysql()
	{return obj.con;}
};

//

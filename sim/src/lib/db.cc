#include "../include/db.h"

namespace DB {

Connection::Connection(const std::string& host, const std::string& user,
			const std::string& password, const std::string& database)
			: conn_(NULL), host_(host), user_(user), password_(password),
			database_(database) {
	connect();
}

void Connection::connect() {
	if(conn_)
		delete conn_;

	conn_ = get_driver_instance()->connect(host_, user_, password_);
	conn_->setSchema(database_);
}

} // namespace DB

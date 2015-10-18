#include "../include/db.h"
#include "../simlib/include/logger.h"

#include <cppconn/driver.h>

using std::string;

namespace DB {

std::mutex Connection::create_connection_lock;

Connection::Connection(const string& host, const string& user,
		const string& password, const string& database)
		: conn_(), host_(host), user_(user), password_(password),
			database_(database) {
	connect();
}

void Connection::connect() {
	// We have to serialize creating connections, because MySQL Connector C++
	// can crush if we won't guard it
	create_connection_lock.lock();
	conn_.reset(get_driver_instance()->connect(host_, user_, password_));
	create_connection_lock.unlock();

	conn_->setSchema(database_);
}

Connection createConnectionUsingPassFile(const string& filename) {
	char *host = nullptr, *user = nullptr, *password = nullptr,
		*database = nullptr;

	FILE *conf = fopen(filename.c_str(), "r");
	if (conf == nullptr)
		throw std::runtime_error(concat("Cannot open file: '", filename, '\'',
			error(errno)));

	// Get credentials
	size_t x1 = 0, x2 = 0, x3 = 0, x4 = 0;
	if (getline(&user, &x1, conf) == -1 ||
			getline(&password, &x2, conf) == -1 ||
			getline(&database, &x3, conf) == -1 ||
			getline(&host, &x4, conf) == -1) {

		// Free resources
		fclose(conf);
		free(host);
		free(user);
		free(password);
		free(database);

		throw std::runtime_error("Failed to get database config");
	}

	fclose(conf);
	user[strlen(user) - 1] = password[strlen(password) - 1] = '\0';
	database[strlen(database) - 1] = host[strlen(host) - 1] = '\0';

	AutoFree<char> f1(user), f2(password), f3(database), f(host);

	// Connect
	try {
		return Connection(host, user, password, database);

	} catch (const std::exception& e) {
		throw std::runtime_error(concat("Failed to connect to database - ",
			e.what()));

	} catch (...) {
		throw std::runtime_error("Failed to connect to database");
	}
}

} // namespace DB

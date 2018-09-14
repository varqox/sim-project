#include <sim/mysql.h>
#include <simlib/config_file.h>

namespace MySQL {

Connection make_conn_with_credential_file(CStringView filename) {
	ConfigFile cf;
	cf.addVars("host", "user", "password", "db");
	cf.loadConfigFromFile(filename);

	cf.getVars().for_each([](auto&& it) {
		if (it.second.isArray())
			THROW("Simfile: variable `", it.first, "` cannot be specified as an"
				" array");
		if (not it.second.isSet())
			THROW("Simfile: variable `", it.first, "` is not set");
	});

	// Connect
	try {
		Connection conn;
		conn.connect(cf["host"].asString(), cf["user"].asString(),
			cf["password"].asString(), cf["db"].asString());
		return conn;

	} catch (const std::exception& e) {
		THROW("Failed to connect to database - ", e.what());
	}
}

} // namespace MySQL

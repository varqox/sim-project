#include "sim/mysql.hh"
#include "simlib/config_file.hh"

namespace MySQL {

Connection make_conn_with_credential_file(FilePath filename) {
    ConfigFile cf;
    cf.add_vars("host", "user", "password", "db");
    cf.load_config_from_file(filename);

    for (const auto& it : cf.get_vars()) {
        if (it.second.is_array()) {
            THROW("Simfile: variable `", it.first, "` cannot be specified as an array");
        }
        if (not it.second.is_set()) {
            THROW("Simfile: variable `", it.first, "` is not set");
        }
    };

    // Connect
    try {
        Connection conn;
        conn.connect(
            cf["host"].as_string(), cf["user"].as_string(), cf["password"].as_string(),
            cf["db"].as_string());
        return conn;

    } catch (const std::exception& e) {
        THROW("Failed to connect to database - ", e.what());
    }
}

} // namespace MySQL

#include "../include/mysql.h"

#ifdef SIMLIB_MYSQL_ENABLED

namespace MySQL {

const Connection::Library Connection::init_ {};

} // namespace MySQL

#endif // SIMLIB_MYSQL_ENABLED

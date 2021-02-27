#pragma once

#include "simlib/mysql/mysql.hh"

namespace job_server {

extern thread_local mysql::Connection mysql;

} // namespace job_server

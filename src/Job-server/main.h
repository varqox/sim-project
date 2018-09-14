#pragma once

#include <sim/sqlite.h>
#include <simlib/mysql.h>

extern thread_local MySQL::Connection mysql;
extern thread_local SQLite::Connection sqlite;

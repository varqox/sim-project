#pragma once

#include <simlib/concat_tostr.hh>
#include <simlib/mysql/mysql.hh>
#include <string>
#include <vector>

namespace sim::db {

struct TableSchema {
    std::string create_table_sql;
};

struct DbSchema {
    std::vector<TableSchema> table_schemas;
};

const DbSchema& get_schema();

std::string normalized(const TableSchema& table_schema);

std::string normalized(const DbSchema& db_schema);

std::vector<std::string> get_all_table_names(mysql::Connection& mysql);

DbSchema get_db_schema(mysql::Connection& mysql);

} // namespace sim::db

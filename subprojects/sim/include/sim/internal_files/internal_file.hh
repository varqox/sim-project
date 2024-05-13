#pragma once

#include <cstdint>
#include <sim/mysql/mysql.hh>
#include <sim/sql/fields/datetime.hh>
#include <sim/sql/sql.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/time.hh>
#include <string>

namespace sim::internal_files {

struct InternalFile {
    uint64_t id;
    sql::fields::Datetime created_at;
};

inline std::string path_of(decltype(InternalFile::id) id) {
    return concat_tostr("internal_files/", id);
}

inline decltype(InternalFile::id)
new_internal_file_id(mysql::Connection& mysql, const std::string& curr_datetime = mysql_date()) {
    auto stmt =
        mysql.execute(sql::InsertInto("internal_files (created_at)").values("?", curr_datetime));
    return stmt.insert_id();
}

} // namespace sim::internal_files

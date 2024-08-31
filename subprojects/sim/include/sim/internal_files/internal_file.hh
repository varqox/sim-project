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
    static constexpr size_t COLUMNS_NUM = 2;
};

inline std::string path_of(decltype(InternalFile::id) id) {
    return concat_tostr("internal_files/", id);
}

inline decltype(InternalFile::id) new_internal_file_id(
    mysql::Connection& mysql, const std::string& current_datetime = utc_mysql_datetime()
) {
    auto stmt =
        mysql.execute(sql::InsertInto("internal_files (created_at)").values("?", current_datetime));
    return stmt.insert_id();
}

} // namespace sim::internal_files

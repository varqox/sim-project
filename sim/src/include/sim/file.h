#pragma once

#include <simlib/mysql.h>

namespace file {

void delete_file(MySQL::Connection& mysql, StringView file_id);

} // namespace submission

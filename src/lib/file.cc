#include <sim/file.h>
#include <simlib/filesystem.h>

namespace file {

void delete_file(MySQL::Connection& mysql, StringView file_id) {
	auto stmt = mysql.prepare("DELETE FROM files WHERE id=?");
	stmt.bindAndExecute(file_id);

	(void)remove(concat("files/", file_id));
}

} // namespace file

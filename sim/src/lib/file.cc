#include <sim/file.h>
#include <simlib/filesystem.h>

namespace file {

void delete_file(MySQL::Connection& mysql, StringView file_id) {
	// TODO: make a transaction for deleting contest files: make a job to delete the real file, then delete the contest file
	mysql.prepare("DELETE FROM files WHERE id=?").bindAndExecute(file_id);
	(void)remove(concat("files/", file_id));
}

} // namespace file

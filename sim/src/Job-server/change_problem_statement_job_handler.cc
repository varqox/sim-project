#include "change_problem_statement_job_handler.h"
#include "main.h"

#include <sim/constants.h>
#include <simlib/sim/problem_package.h>

void ChangeProblemStatementJobHandler::run() {
	STACK_UNWINDING_MARK;

	auto transaction = mysql.start_transaction();

	uint64_t problem_file_id;
	sim::Simfile simfile;
	{
		auto stmt = mysql.prepare("SELECT file_id, simfile FROM problems"
			" WHERE id=?");
		stmt.bindAndExecute(problem_id);
		InplaceBuff<1> simfile_str;
		stmt.res_bind_all(problem_file_id, simfile_str);
		if (not stmt.next())
			return set_failure("Problem with ID = ", problem_id,
				" does not exist");

		simfile = sim::Simfile(simfile_str.to_string());
		simfile.load_all();
	}

	auto pkg_path = internal_file_path(problem_file_id);

	mysql.update("INSERT INTO internal_files VALUES()");
	uint64_t new_file_id = mysql.insert_id();
	auto new_pkg_path = internal_file_path(new_file_id);

	// Replace old statement with new statement

	// Escape info.new_statement_path
	if (info.new_statement_path.empty())
		info.new_statement_path = simfile.statement;
	else
		info.new_statement_path = abspath(info.new_statement_path).erase(0, 1);

	ZipFile src_zip(pkg_path, ZIP_RDONLY);
	auto master_dir = sim::zip_package_master_dir(src_zip);
	auto old_statement_path = concat(master_dir, simfile.statement);
	auto new_statement_path = concat(master_dir, info.new_statement_path);
	auto simfile_path = concat(master_dir, "Simfile");
	if (new_statement_path == simfile_path) {
		return set_failure("Invalid new statement path - it would overwrite"
			" Simfile");
	}

	simfile.statement = info.new_statement_path;
	auto simfile_str = simfile.dump();

	FileRemover new_pkg_remover(new_pkg_path);
	ZipFile dest_zip(new_pkg_path, ZIP_CREATE | ZIP_TRUNCATE);

	auto eno = src_zip.entries_no();
	for (decltype(eno) i = 0; i < eno; ++i) {
		auto entry_name = src_zip.get_name(i);
		if (entry_name == simfile_path)
			dest_zip.file_add(simfile_path, dest_zip.source_buffer(simfile_str));
		else if (entry_name != old_statement_path)
			dest_zip.file_add(entry_name, dest_zip.source_zip(src_zip, i));
	}

	// Add new statement file entry
	dest_zip.file_add(new_statement_path,
		dest_zip.source_file(internal_file_path(job_file_id)));

	dest_zip.close(); // Write all data to the dest_zip

	const auto current_date = mysql_date();
	// Add job to delete old problem file
	mysql.prepare("INSERT INTO jobs(file_id, creator, type, priority, status,"
			" added, aux_id, info, data)"
		" SELECT file_id, NULL, " JTYPE_DELETE_FILE_STR ", ?, "
			JSTATUS_PENDING_STR ", ?, NULL, '', '' FROM problems WHERE id=?")
		.bindAndExecute(priority(JobType::DELETE_FILE), current_date,
			problem_id);

	// Use new package as problem file
	mysql.prepare("UPDATE problems SET file_id=?, simfile=?, last_edit=?"
		" WHERE id=?")
		.bindAndExecute(new_file_id, simfile_str, current_date, problem_id);

	job_done(intentionalUnsafeStringView(info.dump()));

	transaction.commit();
	new_pkg_remover.cancel();
}

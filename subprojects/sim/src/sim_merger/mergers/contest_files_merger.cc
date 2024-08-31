#include "../progress_printer.hh"
#include "../sim_instance.hh"
#include "contest_files_merger.hh"
#include "contests_merger.hh"
#include "internal_files_merger.hh"
#include "users_merger.hh"

#include <optional>
#include <sim/contest_files/contest_file.hh>
#include <sim/mysql/mysql.hh>
#include <sim/random.hh>
#include <sim/sql/sql.hh>
#include <simlib/macros/stack_unwinding.hh>
#include <string>

using sim::contest_files::ContestFile;
using sim::sql::InsertIgnoreInto;
using sim::sql::Select;

namespace mergers {

ContestFilesMerger::ContestFilesMerger(
    const SimInstance& main_sim,
    const SimInstance& other_sim,
    const ContestsMerger& contests_merger,
    const InternalFilesMerger& internal_files_merger,
    const UsersMerger& users_merger
)
: main_sim{main_sim}
, other_sim{other_sim}
, contests_merger{contests_merger}
, internal_files_merger{internal_files_merger}
, users_merger{users_merger} {}

void ContestFilesMerger::merge_and_save() {
    STACK_UNWINDING_MARK;

    // Save other contest_files
    auto other_contest_files_copying_progress_printer =
        ProgressPrinter<decltype(ContestFile::id)>{"progress: copying other contest_file with id:"};
    static_assert(ContestFile::COLUMNS_NUM == 8, "Update the statements below");
    auto stmt = other_sim.mysql.execute(
        Select("id, file_id, contest_id, name, description, file_size, modified, creator")
            .from("contest_files")
            .order_by("id DESC")
    );
    stmt.do_not_store_result(); // minimize memory usage
    ContestFile contest_file;
    stmt.res_bind(
        contest_file.id,
        contest_file.file_id,
        contest_file.contest_id,
        contest_file.name,
        contest_file.description,
        contest_file.file_size,
        contest_file.modified,
        contest_file.creator
    );
    while (stmt.next()) {
        other_contest_files_copying_progress_printer.note_id(contest_file.id);
        for (;;) {
            auto insert_stmt = main_sim.mysql.execute(
                InsertIgnoreInto("contest_files (id, file_id, contest_id, name, "
                                 "description, file_size, modified, creator)")
                    .values(
                        "?, ?, ?, ?, ?, ?, ?, ?",
                        contest_file.id,
                        internal_files_merger.other_id_to_new_id(contest_file.file_id),
                        contests_merger.other_id_to_new_id(contest_file.contest_id),
                        contest_file.name,
                        contest_file.description,
                        contest_file.file_size,
                        contest_file.modified,
                        contest_file.creator
                            ? std::optional{users_merger.other_id_to_new_id(*contest_file.creator)}
                            : std::nullopt
                    )
            );
            if (insert_stmt.affected_rows() == 1) {
                break;
            }
            // Change id on collision
            contest_file.id = sim::generate_random_token(decltype(ContestFile::id)::len);
        }
    }
}

} // namespace mergers

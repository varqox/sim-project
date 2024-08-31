#include "../progress_printer.hh"
#include "../sim_instance.hh"
#include "internal_files_merger.hh"
#include "modify_main_table_ids.hh"
#include "upadte_jobs_using_table_ids.hh"

#include <cstdint>
#include <cstdio>
#include <optional>
#include <sim/internal_files/internal_file.hh>
#include <sim/jobs/job.hh>
#include <sim/merging/merge_ids.hh>
#include <sim/sql/fields/datetime.hh>
#include <sim/sql/sql.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/errmsg.hh>
#include <simlib/file_manip.hh>
#include <simlib/macros/stack_unwinding.hh>
#include <simlib/macros/throw.hh>

using sim::internal_files::InternalFile;
using sim::jobs::Job;
using sim::sql::InsertInto;
using sim::sql::Select;
using sim::sql::Update;

namespace mergers {

std::optional<sim::sql::fields::Datetime>
InternalFilesIdIterator::created_at_upper_bound_of_id(uint64_t id) {
    STACK_UNWINDING_MARK;

    auto stmt =
        mysql.execute(Select("created_at")
                          .from("jobs")
                          .where("type=? AND aux_id=?", Job::Type::DELETE_INTERNAL_FILE, id));
    sim::sql::fields::Datetime created_at;
    stmt.res_bind(created_at);
    if (!stmt.next()) {
        return std::nullopt;
    }

    return created_at;
}

InternalFilesMerger::InternalFilesMerger(const SimInstance& main_sim, const SimInstance& other_sim)
: main_sim{main_sim}
, other_sim{other_sim}
, main_ids_iter{main_sim.mysql} {
    STACK_UNWINDING_MARK;

    auto other_ids_iter = InternalFilesIdIterator{other_sim.mysql};

    merged_ids = sim::merging::merge_ids(main_ids_iter, other_ids_iter);
}

void InternalFilesMerger::merge_and_save() {
    STACK_UNWINDING_MARK;

    modify_main_table_ids(
        main_ids_iter,
        main_sim.mysql,
        "internal_files",
        [this](decltype(InternalFile::id) main_id) { return main_id_to_new_id(main_id); },
        [this](decltype(InternalFile::id) main_id, decltype(InternalFile::id) new_id) {
            if (rename(
                    concat_tostr(main_sim.path, sim::internal_files::path_of(main_id)).c_str(),
                    concat_tostr(main_sim.path, sim::internal_files::path_of(new_id)).c_str()
                ))
            {
                THROW("rename()", errmsg());
            }
        }
    );

    update_jobs_using_table_ids(
        "internal_files",
        main_ids_iter.max_id_plus_one(),
        main_ids_iter.min_id(),
        [this](decltype(InternalFile::id) main_id) { return main_id_to_new_id(main_id); },
        [this](decltype(InternalFile::id) main_id, decltype(InternalFile::id) new_id) {
            main_sim.mysql.execute(
                Update("jobs")
                    .set("aux_id=?", new_id)
                    .where("type=? AND aux_id=?", Job::Type::DELETE_INTERNAL_FILE, main_id)
            );
        }
    );

    // Save other internal_files
    auto other_internal_files_copying_progress_printer =
        ProgressPrinter<decltype(InternalFile::id)>{"progress: copying other internal_file with id:"
        };
    static_assert(InternalFile::COLUMNS_NUM == 2, "update the statements below");
    auto stmt =
        other_sim.mysql.execute(Select("id, created_at").from("internal_files").order_by("id DESC")
        );
    stmt.do_not_store_result(); // minimize memory usage
    InternalFile internal_file;
    stmt.res_bind(internal_file.id, internal_file.created_at);
    while (stmt.next()) {
        other_internal_files_copying_progress_printer.note_id(internal_file.id);
        auto new_id = other_id_to_new_id(internal_file.id);
        main_sim.mysql.execute(InsertInto("internal_files (id, created_at)")
                                   .values("?, ?", new_id, internal_file.created_at));
        if (copy(
                concat_tostr(other_sim.path, sim::internal_files::path_of(internal_file.id))
                    .c_str(),
                concat_tostr(main_sim.path, sim::internal_files::path_of(new_id)).c_str()
            ))
        {
            THROW("copy()", errmsg());
        }
    }
}

} // namespace mergers

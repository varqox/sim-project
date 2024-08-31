#pragma once

#include "../progress_printer.hh"
#include "generic_id_iterator.hh"

#include <cstdint>
#include <sim/mysql/mysql.hh>
#include <sim/sql/sql.hh>
#include <simlib/concat_tostr.hh>

namespace mergers {

template <class MainIdToNewId, class ChangeIdHook = void (*)(uint64_t, uint64_t)>
void modify_main_table_ids(
    GenericIdIterator& main_ids_iter,
    sim::mysql::Connection& main_sim_mysql,
    const char* table_name,
    MainIdToNewId&& main_id_to_new_id,
    ChangeIdHook&& change_id_hook = [](uint64_t, uint64_t) {}
) {
    main_ids_iter.reset();
    auto main_ids_renaming_progress_printer =
        ProgressPrinter<uint64_t>{concat_tostr("progress: modifying main ", table_name, " id:")};
    for (;;) {
        auto id_created_at_opt = main_ids_iter.next_id_desc();
        if (!id_created_at_opt) {
            break;
        }
        auto id = id_created_at_opt->id;
        auto new_id = main_id_to_new_id(id);
        if (id == new_id) {
            break; // no more ids to update (lower ids are unchanged)
        }
        main_ids_renaming_progress_printer.note_id(id);
        main_sim_mysql.execute(sim::sql::Update(table_name).set("id=?", new_id).where("id=?", id));
        change_id_hook(id, new_id);
    }
    main_ids_renaming_progress_printer.done();
}

} // namespace mergers

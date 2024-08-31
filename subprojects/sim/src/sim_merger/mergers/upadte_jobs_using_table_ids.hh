#pragma once

#include "../progress_printer.hh"

#include <cstdint>
#include <simlib/concat_tostr.hh>

namespace mergers {

template <class MainIdToNewId, class JobsUpdater>
void update_jobs_using_table_ids(
    const char* table_name,
    uint64_t main_max_id_plus_one,
    uint64_t main_min_id,
    MainIdToNewId&& main_id_to_new_id,
    JobsUpdater&& jobs_updater
) {
    auto updating_jobs_progress_printer =
        ProgressPrinter<uint64_t>{concat_tostr("progress: updating jobs using ", table_name, " id:")
        };
    for (uint64_t id = main_max_id_plus_one; id != main_min_id;) {
        --id;
        updating_jobs_progress_printer.note_id(id);
        auto new_id = main_id_to_new_id(id);
        if (new_id == id) {
            break; // no more ids to update (lower ids are unchanged)
        }
        jobs_updater(id, new_id);
    }
    updating_jobs_progress_printer.done();
}

} // namespace mergers

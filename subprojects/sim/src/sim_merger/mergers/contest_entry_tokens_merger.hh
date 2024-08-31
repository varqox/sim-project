#pragma once

#include "../sim_instance.hh"
#include "contests_merger.hh"

namespace mergers {

class ContestEntryTokensMerger {
    const SimInstance& main_sim; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    const SimInstance& other_sim; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    const ContestsMerger& contests_merger;

public:
    ContestEntryTokensMerger(
        const SimInstance& main_sim,
        const SimInstance& other_sim,
        const ContestsMerger& contests_merger
    );

    void merge_and_save();
};

} // namespace mergers

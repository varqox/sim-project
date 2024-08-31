#pragma once

#include "../sim_instance.hh"
#include "problems_merger.hh"

namespace mergers {

class ProblemTagsMerger {
    const SimInstance& main_sim; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    const SimInstance& other_sim; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    const ProblemsMerger& problems_merger;

public:
    ProblemTagsMerger(
        const SimInstance& main_sim,
        const SimInstance& other_sim,
        const ProblemsMerger& problems_merger
    );

    void merge_and_save();
};

} // namespace mergers

#pragma once

#include "../sim_instance.hh"
#include "jobs_merger.hh"

namespace mergers {

class MergeProblemsJobsMerger {
    const SimInstance& main_sim; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    const SimInstance& other_sim; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    const JobsMerger& jobs_merger; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)

public:
    MergeProblemsJobsMerger(
        const SimInstance& main_sim, const SimInstance& other_sim, const JobsMerger& jobs_merger
    );

    void merge_and_save();
};

} // namespace mergers

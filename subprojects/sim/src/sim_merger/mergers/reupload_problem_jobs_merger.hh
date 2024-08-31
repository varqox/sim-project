#pragma once

#include "../sim_instance.hh"
#include "internal_files_merger.hh"
#include "jobs_merger.hh"

namespace mergers {

class ReuploadProblemJobsMerger {
    const SimInstance& main_sim; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    const SimInstance& other_sim; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    const JobsMerger& jobs_merger; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    const InternalFilesMerger& internal_files_merger;

public:
    ReuploadProblemJobsMerger(
        const SimInstance& main_sim,
        const SimInstance& other_sim,
        const JobsMerger& jobs_merger,
        const InternalFilesMerger& internal_files_merger
    );

    void merge_and_save();
};

} // namespace mergers

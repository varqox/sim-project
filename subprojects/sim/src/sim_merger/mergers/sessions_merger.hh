#pragma once

#include "../sim_instance.hh"
#include "users_merger.hh"

namespace mergers {

class SessionsMerger {
    const SimInstance& main_sim; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    const SimInstance& other_sim; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    const UsersMerger& users_merger;

public:
    SessionsMerger(
        const SimInstance& main_sim, const SimInstance& other_sim, const UsersMerger& users_merger
    );

    void merge_and_save();
};

} // namespace mergers

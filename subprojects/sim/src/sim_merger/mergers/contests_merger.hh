#pragma once

#include "../sim_instance.hh"
#include "generic_id_iterator.hh"

#include <cstdint>
#include <optional>
#include <sim/contests/contest.hh>
#include <sim/merging/merge_ids.hh>
#include <sim/mysql/mysql.hh>
#include <sim/sql/fields/datetime.hh>

namespace mergers {

class ContestsIdIterator : public GenericIdIterator {
public:
    explicit ContestsIdIterator(sim::mysql::Connection& mysql) noexcept
    : GenericIdIterator(mysql, "contests") {}

    std::optional<sim::sql::fields::Datetime> created_at_upper_bound_of_id(uint64_t id) override;
};

class ContestsMerger {
    const SimInstance& main_sim; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    const SimInstance& other_sim; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)

    ContestsIdIterator main_ids_iter;
    sim::merging::MergedIds merged_ids;

public:
    ContestsMerger(const SimInstance& main_sim, const SimInstance& other_sim);

    void merge_and_save();

    [[nodiscard]] decltype(sim::contests::Contest::id)
    main_id_to_new_id(decltype(sim::contests::Contest::id) main_id) const noexcept {
        return merged_ids.current_id_to_new_id(main_id);
    }

    [[nodiscard]] decltype(sim::contests::Contest::id)
    other_id_to_new_id(decltype(sim::contests::Contest::id) other_id) const noexcept {
        return merged_ids.other_id_to_new_id(other_id);
    }
};

} // namespace mergers

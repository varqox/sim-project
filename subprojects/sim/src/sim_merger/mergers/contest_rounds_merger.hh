#pragma once

#include "../sim_instance.hh"
#include "contests_merger.hh"
#include "generic_id_iterator.hh"

#include <cstdint>
#include <optional>
#include <sim/contest_rounds/contest_round.hh>
#include <sim/merging/merge_ids.hh>
#include <sim/mysql/mysql.hh>
#include <sim/sql/fields/datetime.hh>

namespace mergers {

class ContestRoundsIdIterator : public GenericIdIterator {
public:
    explicit ContestRoundsIdIterator(sim::mysql::Connection& mysql) noexcept
    : GenericIdIterator(mysql, "contest_rounds") {}

    std::optional<sim::sql::fields::Datetime> created_at_upper_bound_of_id(uint64_t id) override;
};

class ContestRoundsMerger {
    const SimInstance& main_sim; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    const SimInstance& other_sim; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    const ContestsMerger& contests_merger;

    ContestRoundsIdIterator main_ids_iter;
    sim::merging::MergedIds merged_ids;

public:
    ContestRoundsMerger(
        const SimInstance& main_sim,
        const SimInstance& other_sim,
        const ContestsMerger& contests_merger
    );

    void merge_and_save();

    [[nodiscard]] decltype(sim::contest_rounds::ContestRound::id)
    main_id_to_new_id(decltype(sim::contest_rounds::ContestRound::id) main_id) const noexcept {
        return merged_ids.current_id_to_new_id(main_id);
    }

    [[nodiscard]] decltype(sim::contest_rounds::ContestRound::id)
    other_id_to_new_id(decltype(sim::contest_rounds::ContestRound::id) other_id) const noexcept {
        return merged_ids.other_id_to_new_id(other_id);
    }
};

} // namespace mergers

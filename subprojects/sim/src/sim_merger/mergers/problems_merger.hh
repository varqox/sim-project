#pragma once

#include "../sim_instance.hh"
#include "generic_id_iterator.hh"
#include "internal_files_merger.hh"
#include "users_merger.hh"

#include <cstdint>
#include <optional>
#include <sim/merging/merge_ids.hh>
#include <sim/mysql/mysql.hh>
#include <sim/problems/problem.hh>
#include <sim/sql/fields/datetime.hh>

namespace mergers {

class ProblemsIdIterator : public GenericIdIterator {
public:
    explicit ProblemsIdIterator(sim::mysql::Connection& mysql) noexcept
    : GenericIdIterator(mysql, "problems") {}

    std::optional<sim::sql::fields::Datetime> created_at_upper_bound_of_id(uint64_t id) override;
};

class ProblemsMerger {
    const SimInstance& main_sim; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    const SimInstance& other_sim; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    const InternalFilesMerger& internal_files_merger;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    const UsersMerger& users_merger;

    ProblemsIdIterator main_ids_iter;
    sim::merging::MergedIds merged_ids;

public:
    ProblemsMerger(
        const SimInstance& main_sim,
        const SimInstance& other_sim,
        const InternalFilesMerger& internal_files_merger,
        const UsersMerger& users_merger
    );

    void merge_and_save();

    [[nodiscard]] decltype(sim::problems::Problem::id)
    main_id_to_new_id(decltype(sim::problems::Problem::id) main_id) const noexcept {
        return merged_ids.current_id_to_new_id(main_id);
    }

    [[nodiscard]] decltype(sim::problems::Problem::id)
    other_id_to_new_id(decltype(sim::problems::Problem::id) other_id) const noexcept {
        return merged_ids.other_id_to_new_id(other_id);
    }
};

} // namespace mergers

#pragma once

#include "../sim_instance.hh"
#include "contest_problems_merger.hh"
#include "contest_rounds_merger.hh"
#include "contests_merger.hh"
#include "generic_id_iterator.hh"
#include "internal_files_merger.hh"
#include "problems_merger.hh"
#include "users_merger.hh"

#include <cstdint>
#include <optional>
#include <sim/merging/merge_ids.hh>
#include <sim/mysql/mysql.hh>
#include <sim/sql/fields/datetime.hh>
#include <sim/submissions/submission.hh>

namespace mergers {

class SubmissionsIdIterator : public GenericIdIterator {
public:
    explicit SubmissionsIdIterator(sim::mysql::Connection& mysql) noexcept
    : GenericIdIterator(mysql, "submissions") {}

    std::optional<sim::sql::fields::Datetime> created_at_upper_bound_of_id(uint64_t id) override;
};

class SubmissionsMerger {
    const SimInstance& main_sim; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    const SimInstance& other_sim; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    const InternalFilesMerger& internal_files_merger;
    const UsersMerger& users_merger; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    const ProblemsMerger& problems_merger;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    const ContestProblemsMerger& contest_problems_merger;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    const ContestRoundsMerger& contest_rounds_merger;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    const ContestsMerger& contests_merger;

    SubmissionsIdIterator main_ids_iter;
    sim::merging::MergedIds merged_ids;

public:
    SubmissionsMerger(
        const SimInstance& main_sim,
        const SimInstance& other_sim,
        const InternalFilesMerger& internal_files_merger,
        const UsersMerger& users_merger,
        const ProblemsMerger& problems_merger,
        const ContestProblemsMerger& contest_problems_merger,
        const ContestRoundsMerger& contest_rounds_merger,
        const ContestsMerger& contests_merger
    );

    void merge_and_save();

    [[nodiscard]] decltype(sim::submissions::Submission::id)
    main_id_to_new_id(decltype(sim::submissions::Submission::id) main_id) const noexcept {
        return merged_ids.current_id_to_new_id(main_id);
    }

    [[nodiscard]] decltype(sim::submissions::Submission::id)
    other_id_to_new_id(decltype(sim::submissions::Submission::id) other_id) const noexcept {
        return merged_ids.other_id_to_new_id(other_id);
    }
};

} // namespace mergers

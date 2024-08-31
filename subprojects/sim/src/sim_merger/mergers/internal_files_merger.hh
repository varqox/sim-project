#pragma once

#include "../sim_instance.hh"
#include "generic_id_iterator.hh"

#include <cstdint>
#include <optional>
#include <sim/internal_files/internal_file.hh>
#include <sim/merging/merge_ids.hh>
#include <sim/mysql/mysql.hh>
#include <sim/sql/fields/datetime.hh>

namespace mergers {

class InternalFilesIdIterator : public GenericIdIterator {
public:
    explicit InternalFilesIdIterator(sim::mysql::Connection& mysql) noexcept
    : GenericIdIterator(mysql, "internal_files") {}

    std::optional<sim::sql::fields::Datetime> created_at_upper_bound_of_id(uint64_t id) override;
};

class InternalFilesMerger {
    const SimInstance& main_sim; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    const SimInstance& other_sim; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)

    InternalFilesIdIterator main_ids_iter;
    sim::merging::MergedIds merged_ids;

public:
    InternalFilesMerger(const SimInstance& main_sim, const SimInstance& other_sim);

    void merge_and_save();

    [[nodiscard]] decltype(sim::internal_files::InternalFile::id)
    main_id_to_new_id(decltype(sim::internal_files::InternalFile::id) main_id) const noexcept {
        return merged_ids.current_id_to_new_id(main_id);
    }

    [[nodiscard]] decltype(sim::internal_files::InternalFile::id)
    other_id_to_new_id(decltype(sim::internal_files::InternalFile::id) other_id) const noexcept {
        return merged_ids.other_id_to_new_id(other_id);
    }
};

} // namespace mergers

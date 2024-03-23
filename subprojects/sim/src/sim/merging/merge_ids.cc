#include <algorithm>
#include <cstdint>
#include <exception>
#include <sim/merging/merge_ids.h>
#include <sim/sql_fields/datetime.hh>
#include <utility>
#include <vector>

using sim::sql_fields::Datetime;

namespace sim::merging {

[[nodiscard]] uint64_t MergedIds::current_id_to_new_id(uint64_t current_id) const noexcept {
    if (current_id < min_new_id) {
        std::terminate();
    }
    if (current_id < first_remapped_current_id) {
        return current_id;
    }
    auto idx = current_id - first_remapped_current_id;
    if (idx >= current_to_new_id.size()) {
        std::terminate();
    }
    return current_to_new_id[idx];
}

[[nodiscard]] uint64_t MergedIds::other_id_to_new_id(uint64_t other_id) const noexcept {
    if (other_id < first_remapped_other_id) {
        std::terminate();
    }
    auto idx = other_id - first_remapped_other_id;
    if (idx >= other_to_new_id.size()) {
        std::terminate();
    }
    return other_to_new_id[idx];
}

[[nodiscard]] MergedIds
merge_ids(IdIterator& current_to_merge_into, IdIterator& other_to_merge_from) {
    const auto current_min_id = current_to_merge_into.min_id();
    const auto current_max_id_plus_one = current_to_merge_into.max_id_plus_one();
    const auto other_min_id = other_to_merge_from.min_id();
    const auto other_max_id_plus_one = other_to_merge_from.max_id_plus_one();

    const auto min_new_id = current_min_id;
    const auto max_new_id_plus_one =
        current_max_id_plus_one + (other_max_id_plus_one - other_min_id);

    auto curr_new_id = max_new_id_plus_one; // Will iterate in decreasing order.

    auto last_remapped_current_id = current_max_id_plus_one;
    auto last_remapped_other_id = other_max_id_plus_one;

    auto current_existing_id = current_to_merge_into.next_id_desc();
    auto other_existing_id = other_to_merge_from.next_id_desc();

    auto last_seen_current_create_at = Datetime{"9999-12-31 23:59:59"};
    auto last_seen_other_create_at = Datetime{"9999-12-31 23:59:59"};

    std::vector<uint64_t> reversed_current_id_to_new_id;
    std::vector<uint64_t> reversed_other_id_to_new_id;

    auto choose_current_id = [&] {
        --last_remapped_current_id;
        reversed_current_id_to_new_id.emplace_back(curr_new_id);
        if (current_existing_id && current_existing_id->id == last_remapped_current_id) {
            last_seen_current_create_at = current_existing_id->created_at;
            current_existing_id = current_to_merge_into.next_id_desc();
        }
    };
    auto choose_other_id = [&] {
        --last_remapped_other_id;
        reversed_other_id_to_new_id.emplace_back(curr_new_id);
        if (other_existing_id && other_existing_id->id == last_remapped_other_id) {
            last_seen_other_create_at = other_existing_id->created_at;
            other_existing_id = other_to_merge_from.next_id_desc();
        }
    };
    auto get_current_id_created_at_upper_bound = [&,
                                                  cached_id = std::optional<uint64_t>{}]() mutable {
        if (cached_id != last_remapped_current_id - 1) {
            cached_id = last_remapped_current_id - 1;
            auto created_at_upper_bound =
                current_to_merge_into.created_at_upper_bound_of_id(*cached_id);
            if (created_at_upper_bound) {
                last_seen_current_create_at =
                    std::min(last_seen_current_create_at, *created_at_upper_bound);
            }
        }
        return last_seen_current_create_at;
    };
    auto get_other_id_created_at_upper_bound = [&,
                                                cached_id = std::optional<uint64_t>{}]() mutable {
        if (cached_id != last_remapped_other_id - 1) {
            cached_id = last_remapped_other_id - 1;
            auto created_at_upper_bound =
                other_to_merge_from.created_at_upper_bound_of_id(*cached_id);
            if (created_at_upper_bound) {
                last_seen_other_create_at =
                    std::min(last_seen_other_create_at, *created_at_upper_bound);
            }
        }
        return last_seen_other_create_at;
    };
    while (last_remapped_other_id > other_min_id) {
        --curr_new_id;
        if (last_remapped_current_id == current_min_id) {
            // Current ids are exhausted, only other ids left.
            choose_other_id();
        } else {
            // Observation: if (last_remapped_current_id - 1) >= current_existing_id->id, then we
            // can assume its created_at is equal to current_existing_id->created_at.
            // Analogously: if (last_remapped_other_id - 1) >= other_existing_id->id, then we
            // can assume its created_at is equal to other_existing_id->created_at.
            auto current_created_at = current_existing_id ? current_existing_id->created_at
                                                          : get_current_id_created_at_upper_bound();
            auto other_created_at = other_existing_id ? other_existing_id->created_at
                                                      : get_other_id_created_at_upper_bound();
            if (other_created_at >= current_created_at) {
                choose_other_id();
            } else {
                choose_current_id();
            }
        }
    }

    std::reverse(reversed_current_id_to_new_id.begin(), reversed_current_id_to_new_id.end());
    std::reverse(reversed_other_id_to_new_id.begin(), reversed_other_id_to_new_id.end());

    MergedIds res;
    res.min_new_id = min_new_id;
    res.max_new_id_plus_one_ = max_new_id_plus_one;
    res.first_remapped_current_id = last_remapped_current_id;
    res.current_to_new_id = std::move(reversed_current_id_to_new_id);
    res.first_remapped_other_id = last_remapped_other_id;
    res.other_to_new_id = std::move(reversed_other_id_to_new_id);
    return res;
}

} // namespace sim::merging

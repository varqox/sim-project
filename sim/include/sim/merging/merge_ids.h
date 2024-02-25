#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <sim/sql_fields/datetime.hh>

namespace sim::merging {

struct IdCreatedAt {
    uint64_t id;
    sql_fields::Datetime created_at;
};

class IdIterator {
public:
    IdIterator() = default;
    IdIterator(const IdIterator&) = default;
    IdIterator(IdIterator&&) = default;
    IdIterator& operator=(const IdIterator&) = default;
    IdIterator& operator=(IdIterator&&) = default;
    virtual ~IdIterator() = default;

    // Returns minimal id number that ever existed or max_id_plus_one() if none existed.
    [[nodiscard]] virtual uint64_t min_id() = 0;

    // Returns maximal id number that ever existed plus 1 or min_id() if none existed.
    [[nodiscard]] virtual uint64_t max_id_plus_one() = 0;

    // Iterates existing ids in the decreasing order. std::nullopt means end of sequence.
    [[nodiscard]] virtual std::optional<IdCreatedAt> next_id_desc() = 0;

    // Returns upper bound on the create_at time for the specified id. For consecutive ids, the
    // upper bound on create_at of the smaller id may be greater than upper bound of create_at on
    // the bigger one.
    [[nodiscard]] virtual std::optional<sql_fields::Datetime>
    created_at_upper_bound_of_id(uint64_t id) = 0;
};

class MergedIds {
public:
    // Terminates if called with an invalid id.
    [[nodiscard]] uint64_t current_id_to_new_id(uint64_t current_id) const noexcept;

    // Terminates if called with an invalid id.
    [[nodiscard]] uint64_t other_id_to_new_id(uint64_t other_id) const noexcept;

    // Returns current_to_merge_into.min_id() if there are no ids.
    [[nodiscard]] uint64_t max_new_id_plus_one() const noexcept { return max_new_id_plus_one_; }

private:
    uint64_t min_new_id;
    uint64_t max_new_id_plus_one_;
    uint64_t first_remapped_current_id;
    std::vector<uint64_t> current_to_new_id;
    uint64_t first_remapped_other_id;
    std::vector<uint64_t> other_to_new_id;

    friend MergedIds merge_ids(IdIterator& current_to_merge_into, IdIterator& other_to_merge_from);
};

[[nodiscard]] MergedIds
merge_ids(IdIterator& current_to_merge_into, IdIterator& other_to_merge_from);

} // namespace sim::merging

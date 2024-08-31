#pragma once

#include "../sim_instance.hh"
#include "generic_id_iterator.hh"

#include <cstdint>
#include <optional>
#include <sim/merging/merge_ids.hh>
#include <sim/mysql/mysql.hh>
#include <sim/sql/fields/datetime.hh>
#include <sim/users/user.hh>
#include <unordered_map>
#include <vector>

namespace mergers {

class UsersIdIterator : public GenericIdIterator {
public:
    explicit UsersIdIterator(sim::mysql::Connection& mysql) noexcept
    : GenericIdIterator{mysql, "users"} {}

    std::optional<sim::sql::fields::Datetime> created_at_upper_bound_of_id(uint64_t id) override;
};

class UsersMerger {
    const SimInstance& main_sim; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    const SimInstance& other_sim; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    sim::merging::MergedIds merged_ids;
    std::vector<decltype(sim::users::User::id)> merged_ids_of_merged_other_users;
    std::unordered_map<decltype(sim::users::User::id), decltype(sim::users::User::id)>
        other_id_to_main_id_with_the_same_username;

    UsersIdIterator main_ids_iter;

public:
    UsersMerger(const SimInstance& main_sim, const SimInstance& other_sim);

    void merge_and_save();

    [[nodiscard]] decltype(sim::users::User::id) main_id_to_new_id(decltype(sim::users::User::id
    ) main_id) const noexcept;

    [[nodiscard]] decltype(sim::users::User::id) other_id_to_new_id(decltype(sim::users::User::id
    ) other_id) const noexcept;

private:
    [[nodiscard]] decltype(sim::users::User::id) shifted_new_id(decltype(sim::users::User::id
    ) new_id) const noexcept;
};

} // namespace mergers

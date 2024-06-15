#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <iostream>
#include <map>
#include <optional>
#include <sim/merging/merge_ids.hh>
#include <sim/sql/fields/datetime.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/random.hh>
#include <simlib/random_bit_vector.hh>

using std::cerr;

class TestIdIterator : public sim::merging::IdIterator {
public:
    explicit TestIdIterator(
        uint64_t min_id,
        uint64_t max_id_plus_one,
        std::map<uint64_t, sim::sql::fields::Datetime> id_to_datetime,
        std::vector<sim::merging::IdCreatedAt> ids
    )
    : min_id_{min_id}
    , max_id_plus_one_{max_id_plus_one}
    , id_to_datetime{std::move(id_to_datetime)}
    , ids{std::move(ids)}
    , prev_ids_idx{this->ids.size()} {}

    [[nodiscard]] uint64_t min_id() override { return min_id_; }

    [[nodiscard]] uint64_t max_id_plus_one() override { return max_id_plus_one_; }

    [[nodiscard]] std::optional<sim::merging::IdCreatedAt> next_id_desc() override {
        if (prev_ids_idx == 0) {
            return std::nullopt;
        }
        return ids[--prev_ids_idx];
    }

    [[nodiscard]] std::optional<sim::sql::fields::Datetime> created_at_upper_bound_of_id(uint64_t id
    ) override {
        auto it = id_to_datetime.find(id);
        if (it == id_to_datetime.end()) {
            return std::nullopt;
        }
        return it->second;
    }

private:
    uint64_t min_id_;
    uint64_t max_id_plus_one_;
    std::map<uint64_t, sim::sql::fields::Datetime> id_to_datetime;
    std::vector<sim::merging::IdCreatedAt> ids;
    size_t prev_ids_idx;
};

// NOLINTNEXTLINE
TEST(merge_ids, random) {
    size_t tries = 0;
    constexpr size_t REPS = 200;
    for (size_t rep = 0; rep < REPS; ++rep) {
        uint64_t min_id = get_random(0, 10);
        uint64_t max_id_plus_one = get_random(min_id, min_id + 20);
        size_t ids_num = max_id_plus_one - min_id;
        // Get random sequence until it is valid.
        {
        try_again:
            ++tries;
            // origin: current or other
            auto comes_from_current = RandomBitVector{ids_num};
            auto is_deleted = RandomBitVector{ids_num};

            constexpr int MIN_CREATED_AT = 0;
            constexpr int INF_CREATED_AT = 11;
            const int max_created_at = get_random(MIN_CREATED_AT, INF_CREATED_AT - 1);
            std::vector<int> created_at(ids_num);
            for (auto& x : created_at) {
                x = get_random(MIN_CREATED_AT, max_created_at);
            }
            std::sort(created_at.begin(), created_at.end());

            // Fix created_at
            std::optional<int> last_current_non_deleted_created_at;
            std::optional<int> last_other_non_deleted_created_at;
            for (size_t i = 0; i < ids_num; ++i) {
                if (is_deleted.get(i)) {
                    // It is assumed that deleted records have the same created_at as the last
                    // non-deleted record preceding it.
                    if (comes_from_current.get(i) && last_current_non_deleted_created_at) {
                        created_at[i] = *last_current_non_deleted_created_at;
                    } else if (!comes_from_current.get(i) && last_other_non_deleted_created_at) {
                        created_at[i] = *last_other_non_deleted_created_at;
                    }
                    if (i > 0 && created_at[i - 1] > created_at[i]) {
                        goto try_again;
                    }
                } else {
                    if (comes_from_current.get(i)) {
                        last_current_non_deleted_created_at = created_at[i];
                    } else {
                        last_other_non_deleted_created_at = created_at[i];
                    }
                }
            }

            for (size_t i = 1; i < ids_num; ++i) {
                if (!comes_from_current.get(i - 1) && comes_from_current.get(i) &&
                    created_at[i - 1] == created_at[i])
                {
                    // Other takes precedence over current when giving ids in descending order.
                    goto try_again;
                }
            }

            // Randomize and delete some created_at
            std::vector<bool> is_created_at_deleted(ids_num, false);
            {
                std::optional<int> last_current_created_at;
                std::optional<int> last_other_created_at;
                bool saw_current = false;
                size_t i = ids_num;
                while (i > 0) {
                    --i;
                    if (is_deleted.get(i)) {
                        if (comes_from_current.get(i)) {
                            if (created_at[i] == last_current_created_at ||
                                (!last_current_created_at && !last_other_created_at))
                            {
                                if (get_random(0, 1)) {
                                    is_created_at_deleted[i] = true;
                                } else {
                                    created_at[i] = get_random(created_at[i], max_created_at);
                                }
                            }
                            if (!is_created_at_deleted[i]) {
                                last_current_created_at = created_at[i];
                            }
                            saw_current = true;
                        } else {
                            if (created_at[i] == last_other_created_at ||
                                (!last_other_created_at && !last_current_created_at && !saw_current
                                ))
                            {
                                if (get_random(0, 1)) {
                                    is_created_at_deleted[i] = true;
                                } else {
                                    created_at[i] = get_random(created_at[i], max_created_at);
                                }
                            }
                            if (!is_created_at_deleted[i]) {
                                last_other_created_at = created_at[i];
                            }
                        }
                    } else {
                        if (comes_from_current.get(i)) {
                            last_current_created_at = created_at[i];
                            saw_current = true;
                        } else {
                            last_other_created_at = created_at[i];
                        }
                    }
                }
            }

            auto int_to_datetime = [](int x) {
                assert(0 <= x && x < 1000);
                return sim::sql::fields::Datetime{concat_tostr(x + 2000, "-01-01 00:00:00")};
            };

            /* Split to current and other */

            uint64_t current_min_id = min_id;
            uint64_t current_max_id_plus_one = current_min_id;
            std::map<uint64_t, uint64_t> current_id_to_new_id;
            std::map<uint64_t, sim::sql::fields::Datetime> current_id_to_datetime;
            std::vector<sim::merging::IdCreatedAt> current_ids;

            uint64_t other_min_id = get_random(0, 10);
            uint64_t other_max_id_plus_one = other_min_id;
            std::map<uint64_t, uint64_t> other_id_to_new_id;
            std::map<uint64_t, sim::sql::fields::Datetime> other_id_to_datetime;
            std::vector<sim::merging::IdCreatedAt> other_ids;

            for (size_t i = 0; i < max_id_plus_one - min_id; ++i) {
                if (comes_from_current.get(i)) {
                    current_id_to_new_id[current_max_id_plus_one] = i + min_id;
                    if (!is_created_at_deleted[i]) {
                        current_id_to_datetime[current_max_id_plus_one] =
                            int_to_datetime(created_at[i]);
                    }
                    if (!is_deleted.get(i)) {
                        current_ids.emplace_back(sim::merging::IdCreatedAt{
                            .id = current_max_id_plus_one,
                            .created_at = int_to_datetime(created_at[i]),
                        });
                    }
                    ++current_max_id_plus_one;
                } else {
                    other_id_to_new_id[other_max_id_plus_one] = i + min_id;
                    if (!is_created_at_deleted[i]) {
                        other_id_to_datetime[other_max_id_plus_one] =
                            int_to_datetime(created_at[i]);
                    }
                    if (!is_deleted.get(i)) {
                        other_ids.emplace_back(sim::merging::IdCreatedAt{
                            .id = other_max_id_plus_one,
                            .created_at = int_to_datetime(created_at[i]),
                        });
                    }
                    ++other_max_id_plus_one;
                }
            }

            auto current_id_iter = TestIdIterator{
                current_min_id, current_max_id_plus_one, current_id_to_datetime, current_ids
            };
            auto other_id_iter = TestIdIterator{
                other_min_id, other_max_id_plus_one, other_id_to_datetime, other_ids
            };

            auto merged_ids = sim::merging::merge_ids(current_id_iter, other_id_iter);

            EXPECT_EQ(merged_ids.max_new_id_plus_one(), max_id_plus_one);
            for (auto [current_id, new_id] : current_id_to_new_id) {
                EXPECT_EQ(merged_ids.current_id_to_new_id(current_id), new_id);
            }
            for (auto [other_id, new_id] : other_id_to_new_id) {
                EXPECT_EQ(merged_ids.other_id_to_new_id(other_id), new_id);
            }

            if (::testing::Test::HasFailure()) {
                cerr << "min_id: " << min_id << '\n';
                cerr << "max_id_plus_one: " << max_id_plus_one << '\n';
                cerr << "ids: {" << '\n';
                for (size_t i = 0; i < max_id_plus_one - min_id; ++i) {
                    cerr << "  " << std::setw(3) << i + min_id << ' '
                         << int_to_datetime(created_at[i]) << " " << std::setw(7)
                         << (comes_from_current.get(i) ? "current" : "other") << ' '
                         << (is_deleted.get(i) ? "deleted" : "") << '\n';
                }
                cerr << "}" << '\n';
                cerr << "current (min_id = " << current_min_id
                     << " max_id_plus_one = " << current_max_id_plus_one << "): {" << '\n';
                auto current_ids_it = current_ids.begin();
                for (size_t id = current_min_id; id < current_max_id_plus_one; ++id) {
                    bool deleted = true;
                    auto datetime_str = [&]() -> std::string {
                        if (current_ids_it != current_ids.end() && current_ids_it->id == id) {
                            deleted = false;
                            return (current_ids_it++)->created_at;
                        }
                        auto it = current_id_to_datetime.find(id);
                        if (it != current_id_to_datetime.end()) {
                            return it->second;
                        }
                        return std::string{"unknown"};
                    }();
                    cerr << "  " << std::setw(3) << id << ' ' << std::setw(19) << datetime_str
                         << ' ' << std::setw(7) << (deleted ? "deleted" : "") << " -> "
                         << current_id_to_new_id[id]
                         << " (merge tells: " << merged_ids.current_id_to_new_id(id) << ")" << '\n';
                }
                cerr << "}" << '\n';
                cerr << "other (min_id = " << other_min_id
                     << " max_id_plus_one = " << other_max_id_plus_one << "): {" << '\n';
                auto other_ids_it = other_ids.begin();
                for (size_t id = other_min_id; id < other_max_id_plus_one; ++id) {
                    bool deleted = true;
                    auto datetime_str = [&]() -> std::string {
                        if (other_ids_it != other_ids.end() && other_ids_it->id == id) {
                            deleted = false;
                            return (other_ids_it++)->created_at;
                        }
                        auto it = other_id_to_datetime.find(id);
                        if (it != other_id_to_datetime.end()) {
                            return it->second;
                        }
                        return std::string{"unknown"};
                    }();
                    cerr << "  " << std::setw(3) << id << ' ' << std::setw(19) << datetime_str
                         << ' ' << std::setw(7) << (deleted ? "deleted" : "") << " -> "
                         << other_id_to_new_id[id]
                         << " (merge tells: " << merged_ids.other_id_to_new_id(id) << ")" << '\n';
                }
                cerr << "}" << '\n';
                break;
            }
        }
    }
    cerr << "tries: " << tries << '\n';
}

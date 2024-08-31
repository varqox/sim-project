#pragma once

#include <cstdint>
#include <optional>
#include <sim/merging/merge_ids.hh>
#include <sim/mysql/mysql.hh>
#include <sim/sql/fields/datetime.hh>
#include <string_view>

namespace mergers {

class GenericIdIterator : public sim::merging::IdIterator {
protected:
    sim::mysql::Connection& mysql; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    std::string_view table_name;
    std::optional<uint64_t> auto_increment_value;
    std::optional<uint64_t> last_seen_id;
    std::optional<sim::mysql::Statement> id_select_stmt;
    uint64_t id;
    sim::sql::fields::Datetime created_at;

public:
    explicit GenericIdIterator(
        sim::mysql::Connection& mysql, const std::string_view& table_name
    ) noexcept
    : mysql{mysql}
    , table_name{table_name} {}

    GenericIdIterator(const GenericIdIterator&) = delete;
    GenericIdIterator(GenericIdIterator&&) = delete;
    GenericIdIterator& operator=(const GenericIdIterator&) = delete;
    GenericIdIterator& operator=(GenericIdIterator&&) = delete;
    ~GenericIdIterator() override = default;

    uint64_t min_id() final { return 1; }

    uint64_t max_id_plus_one() final;

    std::optional<sim::merging::IdCreatedAt> next_id_desc() override;

    void reset();
};

} // namespace mergers

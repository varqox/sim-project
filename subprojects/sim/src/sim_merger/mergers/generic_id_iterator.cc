#include "generic_id_iterator.hh"

#include <cstdint>
#include <optional>
#include <sim/merging/merge_ids.hh>
#include <sim/sql/sql.hh>
#include <simlib/throw_assert.hh>

namespace mergers {

uint64_t GenericIdIterator::max_id_plus_one() {
    if (auto_increment_value) {
        return *auto_increment_value;
    }

    auto stmt = mysql.execute(sim::sql::Select("AUTO_INCREMENT")
                                  .from("information_schema.tables")
                                  .where("TABLE_SCHEMA=DATABASE() AND TABLE_NAME=?", table_name));
    uint64_t res;
    stmt.res_bind(res);
    throw_assert(stmt.next());
    auto_increment_value = res;
    return res;
}

std::optional<sim::merging::IdCreatedAt> GenericIdIterator::next_id_desc() {
    if (!last_seen_id) {
        last_seen_id = max_id_plus_one();
    }
    if (!id_select_stmt || !id_select_stmt->next()) {
        // Starting iterating or the current range has ended
        id_select_stmt.emplace(mysql.execute(sim::sql::Select("id, created_at")
                                                 .from(table_name)
                                                 .where("id<?", last_seen_id)
                                                 .order_by("id DESC")
                                                 .limit("256")));
        id_select_stmt->res_bind(id, created_at);
        if (!id_select_stmt->next()) {
            // No more ids
            id_select_stmt.reset();
            return std::nullopt;
        }
    }

    last_seen_id = id;
    return sim::merging::IdCreatedAt{
        .id = id,
        .created_at = created_at,
    };
}

void GenericIdIterator::reset() {
    last_seen_id = std::nullopt;
    id_select_stmt.reset();
}

} // namespace mergers

#include <sim/db/schema.hh>
#include <sim/db/tables.hh>
#include <simlib/throw_assert.hh>
#include <string_view>

namespace sim::db {

const std::vector<std::string_view>& get_tables() {
    static const auto tables = [] {
        std::vector<std::string_view> tables;
        const auto& db_schema = get_schema();
        tables.reserve(db_schema.table_schemas.size());
        for (const auto& table_schema : db_schema.table_schemas) {
            // Extract table name
            auto quote_start = table_schema.create_table_sql.find('`');
            throw_assert(quote_start != decltype(table_schema.create_table_sql)::npos);
            auto quote_end = table_schema.create_table_sql.find('`', quote_start + 1);
            throw_assert(quote_end != decltype(table_schema.create_table_sql)::npos);
            auto table_name = std::string_view{
                table_schema.create_table_sql.data() + quote_start + 1, quote_end - quote_start - 1
            };
            tables.emplace_back(table_name);
        }
        return tables;
    }();
    return tables;
}

} // namespace sim::db

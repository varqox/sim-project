#include <gtest/gtest.h>
#include <sim/sql/sql.hh>
#include <stdexcept>
#include <string_view>

using sim::sql::Condition;
using sim::sql::DeleteFrom;
using sim::sql::InsertIgnoreInto;
using sim::sql::InsertInto;
using sim::sql::Select;
using sim::sql::SqlWithParams;
using sim::sql::Update;

#define SQL_EQ(sql_expr, sql_str) \
    EXPECT_EQ(SqlWithParams{sql_expr}.get_sql(), std::string_view{sql_str});

// NOLINTNEXTLINE
TEST(sql, constructing_sqls_SqlWithParams) {
    SQL_EQ(sim::sql::SqlWithParams("SELECT ?", 1), "SELECT ?");
}

TEST(sql, constructing_sqls_Select) {
    SQL_EQ(Select("a, b, c").from("abc"), "SELECT a, b, c FROM abc");
}

TEST(sql, constructing_sqls_SelectFrom) {
    SQL_EQ(
        Select("a, b, c").from("abc").left_join("xyz").on("x=?", 1),
        "SELECT a, b, c FROM abc LEFT JOIN xyz ON x=?"
    );

    SQL_EQ(
        Select("a, b, c").from("abc").left_join(Select("x, y").from("xyz"), "xy").on("x=a"),
        "SELECT a, b, c FROM abc LEFT JOIN (SELECT x, y FROM xyz) xy ON x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .left_join(Select("x, y").from("xyz").inner_join("aaa").on("aaa.a=x"), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc LEFT JOIN (SELECT x, y FROM xyz INNER JOIN aaa ON aaa.a=x) xy ON "
        "x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .left_join(Select("x, y").from("xyz").where("z=42"), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc LEFT JOIN (SELECT x, y FROM xyz WHERE z=42) xy ON x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .left_join(Select("x, y").from("xyz").group_by("x, y"), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc LEFT JOIN (SELECT x, y FROM xyz GROUP BY x, y) xy ON x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .left_join(Select("x, y").from("xyz").order_by("x, y"), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc LEFT JOIN (SELECT x, y FROM xyz ORDER BY x, y) xy ON x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .left_join(Select("x, y").from("xyz").limit("?", 10), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc LEFT JOIN (SELECT x, y FROM xyz LIMIT ?) xy ON x=a"
    );

    SQL_EQ(
        Select("a, b, c").from("abc").right_join("xyz").on("x=? AND y=?", 1, false),
        "SELECT a, b, c FROM abc RIGHT JOIN xyz ON x=? AND y=?"
    );

    SQL_EQ(
        Select("a, b, c").from("abc").right_join(Select("x, y").from("xyz"), "xy").on("x=a"),
        "SELECT a, b, c FROM abc RIGHT JOIN (SELECT x, y FROM xyz) xy ON x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .right_join(Select("x, y").from("xyz").inner_join("aaa").on("aaa.a=x"), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc RIGHT JOIN (SELECT x, y FROM xyz INNER JOIN aaa ON aaa.a=x) xy ON "
        "x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .right_join(Select("x, y").from("xyz").where("z=42"), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc RIGHT JOIN (SELECT x, y FROM xyz WHERE z=42) xy ON x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .right_join(Select("x, y").from("xyz").group_by("x, y"), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc RIGHT JOIN (SELECT x, y FROM xyz GROUP BY x, y) xy ON x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .right_join(Select("x, y").from("xyz").order_by("x, y"), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc RIGHT JOIN (SELECT x, y FROM xyz ORDER BY x, y) xy ON x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .right_join(Select("x, y").from("xyz").limit("?", 10), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc RIGHT JOIN (SELECT x, y FROM xyz LIMIT ?) xy ON x=a"
    );

    SQL_EQ(
        Select("a, b, c").from("abc").inner_join("xyz").on(Condition("x=a")),
        "SELECT a, b, c FROM abc INNER JOIN xyz ON x=a"
    );

    SQL_EQ(
        Select("a, b, c").from("abc").inner_join(Select("x, y").from("xyz"), "xy").on("x=a"),
        "SELECT a, b, c FROM abc INNER JOIN (SELECT x, y FROM xyz) xy ON x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .inner_join(Select("x, y").from("xyz").inner_join("aaa").on("aaa.a=x"), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc INNER JOIN (SELECT x, y FROM xyz INNER JOIN aaa ON aaa.a=x) xy ON "
        "x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .inner_join(Select("x, y").from("xyz").where("z=42"), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc INNER JOIN (SELECT x, y FROM xyz WHERE z=42) xy ON x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .inner_join(Select("x, y").from("xyz").group_by("x, y"), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc INNER JOIN (SELECT x, y FROM xyz GROUP BY x, y) xy ON x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .inner_join(Select("x, y").from("xyz").order_by("x, y"), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc INNER JOIN (SELECT x, y FROM xyz ORDER BY x, y) xy ON x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .inner_join(Select("x, y").from("xyz").limit("?", 10), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc INNER JOIN (SELECT x, y FROM xyz LIMIT ?) xy ON x=a"
    );

    SQL_EQ(Select("a, b, c").from("abc").where("a=42"), "SELECT a, b, c FROM abc WHERE a=42");

    SQL_EQ(Select("a, b, c").from("abc").where("a=?", 42), "SELECT a, b, c FROM abc WHERE a=?");

    SQL_EQ(
        Select("a, b, c").from("abc").where("a=? AND b=?", 42, false),
        "SELECT a, b, c FROM abc WHERE a=? AND b=?"
    );

    SQL_EQ(
        Select("a, b, c").from("abc").where(Condition("a=42")), "SELECT a, b, c FROM abc WHERE a=42"
    );

    SQL_EQ(Select("a, b, c").from("abc").group_by("x"), "SELECT a, b, c FROM abc GROUP BY x");

    SQL_EQ(Select("a, b, c").from("abc").order_by("x"), "SELECT a, b, c FROM abc ORDER BY x");

    SQL_EQ(Select("a, b, c").from("abc").limit("10"), "SELECT a, b, c FROM abc LIMIT 10");
}

TEST(sql, constructing_sqls_Condition) {
    SQL_EQ(
        Select("a").from("abc").where(Condition("b=42") && Condition("c=7")),
        "SELECT a FROM abc WHERE b=42 AND c=7"
    );

    SQL_EQ(
        Select("a").from("abc").where(Condition("b=42") || Condition("c=7")),
        "SELECT a FROM abc WHERE (b=42 OR c=7)"
    );

    SQL_EQ(
        Select("a").from("abc").where((std::optional<Condition<>>{std::nullopt} &&
                                       std::optional<Condition<>>{std::nullopt})
                                          .value_or(Condition("TRUE"))),
        "SELECT a FROM abc WHERE TRUE"
    );

    SQL_EQ(
        Select("a").from("abc").where((std::optional{Condition("b=42")} &&
                                       std::optional<Condition<>>{std::nullopt})
                                          .value_or(Condition("TRUE"))),
        "SELECT a FROM abc WHERE b=42"
    );

    SQL_EQ(
        Select("a").from("abc").where((std::optional<Condition<>>{std::nullopt} &&
                                       std::optional{Condition("c=7")})
                                          .value_or(Condition("TRUE"))),
        "SELECT a FROM abc WHERE c=7"
    );

    SQL_EQ(
        Select("a").from("abc").where((std::optional{Condition("b=42")} &&
                                       std::optional{Condition("c=7")})
                                          .value_or(Condition("TRUE"))),
        "SELECT a FROM abc WHERE b=42 AND c=7"
    );

    SQL_EQ(
        Select("a").from("abc").where((std::optional<Condition<>>{std::nullopt} ||
                                       std::optional<Condition<>>{std::nullopt})
                                          .value_or(Condition("TRUE"))),
        "SELECT a FROM abc WHERE TRUE"
    );

    SQL_EQ(
        Select("a").from("abc").where((std::optional{Condition("b=42")} ||
                                       std::optional<Condition<>>{std::nullopt})
                                          .value_or(Condition("TRUE"))),
        "SELECT a FROM abc WHERE b=42"
    );

    SQL_EQ(
        Select("a").from("abc").where((std::optional<Condition<>>{std::nullopt} ||
                                       std::optional{Condition("c=7")})
                                          .value_or(Condition("TRUE"))),
        "SELECT a FROM abc WHERE c=7"
    );

    SQL_EQ(
        Select("a").from("abc").where((std::optional{Condition("b=42")} ||
                                       std::optional{Condition("c=7")})
                                          .value_or(Condition("TRUE"))),
        "SELECT a FROM abc WHERE (b=42 OR c=7)"
    );
}

TEST(sql, constructing_sqls_SelectJoinOn) {
    SQL_EQ(
        Select("a, b, c").from("abc").inner_join("ooo").on("ooo.a=a").left_join("xyz").on("xyz.x=a"
        ),
        "SELECT a, b, c FROM abc INNER JOIN ooo ON ooo.a=a LEFT JOIN xyz ON xyz.x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .inner_join("ooo")
            .on("ooo.a=a")
            .left_join(Select("x, y").from("xyz"), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc INNER JOIN ooo ON ooo.a=a LEFT JOIN (SELECT x, y FROM xyz) xy ON "
        "x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .inner_join("ooo")
            .on("ooo.a=a")
            .left_join(Select("x, y").from("xyz").inner_join("aaa").on("aaa.a=x"), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc INNER JOIN ooo ON ooo.a=a LEFT JOIN (SELECT x, y FROM xyz INNER "
        "JOIN aaa ON aaa.a=x) xy ON x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .inner_join("ooo")
            .on("ooo.a=a")
            .left_join(Select("x, y").from("xyz").where("z=42"), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc INNER JOIN ooo ON ooo.a=a LEFT JOIN (SELECT x, y FROM xyz WHERE "
        "z=42) xy ON x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .inner_join("ooo")
            .on("ooo.a=a")
            .left_join(Select("x, y").from("xyz").group_by("x, y"), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc INNER JOIN ooo ON ooo.a=a LEFT JOIN (SELECT x, y FROM xyz GROUP "
        "BY x, y) xy ON x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .inner_join("ooo")
            .on("ooo.a=a")
            .left_join(Select("x, y").from("xyz").order_by("x, y"), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc INNER JOIN ooo ON ooo.a=a LEFT JOIN (SELECT x, y FROM xyz ORDER "
        "BY x, y) xy ON x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .inner_join("ooo")
            .on("ooo.a=a")
            .left_join(Select("x, y").from("xyz").limit("?", 10), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc INNER JOIN ooo ON ooo.a=a LEFT JOIN (SELECT x, y FROM xyz LIMIT "
        "?) xy ON x=a"
    );

    SQL_EQ(
        Select("a, b, c").from("abc").inner_join("ooo").on("ooo.a=a").right_join("xyz").on("xyz.x=a"
        ),
        "SELECT a, b, c FROM abc INNER JOIN ooo ON ooo.a=a RIGHT JOIN xyz ON xyz.x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .inner_join("ooo")
            .on("ooo.a=a")
            .right_join(Select("x, y").from("xyz"), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc INNER JOIN ooo ON ooo.a=a RIGHT JOIN (SELECT x, y FROM xyz) xy ON "
        "x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .inner_join("ooo")
            .on("ooo.a=a")
            .right_join(Select("x, y").from("xyz").inner_join("aaa").on("aaa.a=x"), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc INNER JOIN ooo ON ooo.a=a RIGHT JOIN (SELECT x, y FROM xyz INNER "
        "JOIN aaa ON aaa.a=x) xy ON x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .inner_join("ooo")
            .on("ooo.a=a")
            .right_join(Select("x, y").from("xyz").where("z=42"), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc INNER JOIN ooo ON ooo.a=a RIGHT JOIN (SELECT x, y FROM xyz WHERE "
        "z=42) xy ON x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .inner_join("ooo")
            .on("ooo.a=a")
            .right_join(Select("x, y").from("xyz").group_by("x, y"), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc INNER JOIN ooo ON ooo.a=a RIGHT JOIN (SELECT x, y FROM xyz GROUP "
        "BY x, y) xy ON x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .inner_join("ooo")
            .on("ooo.a=a")
            .right_join(Select("x, y").from("xyz").order_by("x, y"), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc INNER JOIN ooo ON ooo.a=a RIGHT JOIN (SELECT x, y FROM xyz ORDER "
        "BY x, y) xy ON x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .inner_join("ooo")
            .on("ooo.a=a")
            .right_join(Select("x, y").from("xyz").limit("?", 10), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc INNER JOIN ooo ON ooo.a=a RIGHT JOIN (SELECT x, y FROM xyz LIMIT "
        "?) xy ON x=a"
    );

    SQL_EQ(
        Select("a, b, c").from("abc").inner_join("ooo").on("ooo.a=a").inner_join("xyz").on("xyz.x=a"
        ),
        "SELECT a, b, c FROM abc INNER JOIN ooo ON ooo.a=a INNER JOIN xyz ON xyz.x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .inner_join("ooo")
            .on("ooo.a=a")
            .inner_join(Select("x, y").from("xyz"), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc INNER JOIN ooo ON ooo.a=a INNER JOIN (SELECT x, y FROM xyz) xy ON "
        "x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .inner_join("ooo")
            .on("ooo.a=a")
            .inner_join(Select("x, y").from("xyz").inner_join("aaa").on("aaa.a=x"), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc INNER JOIN ooo ON ooo.a=a INNER JOIN (SELECT x, y FROM xyz INNER "
        "JOIN aaa ON aaa.a=x) xy ON x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .inner_join("ooo")
            .on("ooo.a=a")
            .inner_join(Select("x, y").from("xyz").where("z=42"), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc INNER JOIN ooo ON ooo.a=a INNER JOIN (SELECT x, y FROM xyz WHERE "
        "z=42) xy ON x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .inner_join("ooo")
            .on("ooo.a=a")
            .inner_join(Select("x, y").from("xyz").group_by("x, y"), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc INNER JOIN ooo ON ooo.a=a INNER JOIN (SELECT x, y FROM xyz GROUP "
        "BY x, y) xy ON x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .inner_join("ooo")
            .on("ooo.a=a")
            .inner_join(Select("x, y").from("xyz").order_by("x, y"), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc INNER JOIN ooo ON ooo.a=a INNER JOIN (SELECT x, y FROM xyz ORDER "
        "BY x, y) xy ON x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .inner_join("ooo")
            .on("ooo.a=a")
            .inner_join(Select("x, y").from("xyz").limit("?", 10), "xy")
            .on("x=a"),
        "SELECT a, b, c FROM abc INNER JOIN ooo ON ooo.a=a INNER JOIN (SELECT x, y FROM xyz LIMIT "
        "?) xy ON x=a"
    );

    SQL_EQ(
        Select("a, b, c")
            .from("abc")
            .inner_join("xyz")
            .on(Condition("x=a"))
            .where(Condition("b=42")),
        "SELECT a, b, c FROM abc INNER JOIN xyz ON x=a WHERE b=42"
    );

    SQL_EQ(
        Select("a, b, c").from("abc").inner_join("xyz").on("x=a").group_by("y"),
        "SELECT a, b, c FROM abc INNER JOIN xyz ON x=a GROUP BY y"
    );

    SQL_EQ(
        Select("a, b, c").from("abc").inner_join("xyz").on("x=a").order_by("x"),
        "SELECT a, b, c FROM abc INNER JOIN xyz ON x=a ORDER BY x"
    );

    SQL_EQ(
        Select("a, b, c").from("abc").inner_join("xyz").on("x=a").limit("10"),
        "SELECT a, b, c FROM abc INNER JOIN xyz ON x=a LIMIT 10"
    );
}

TEST(sql, constructing_sqls_SelectWhere) {
    SQL_EQ(
        Select("a, b, c").from("abc").where("a=1").group_by("x"),
        "SELECT a, b, c FROM abc WHERE a=1 GROUP BY x"
    );

    SQL_EQ(
        Select("a, b, c").from("abc").where("b=42").order_by("x"),
        "SELECT a, b, c FROM abc WHERE b=42 ORDER BY x"
    );

    SQL_EQ(
        Select("a, b, c").from("abc").where("b=42").limit("10"),
        "SELECT a, b, c FROM abc WHERE b=42 LIMIT 10"
    );
}

TEST(sql, constructing_sqls_SelectGroupBy) {
    SQL_EQ(
        Select("a, b, c").from("abc").group_by("b").order_by("x"),
        "SELECT a, b, c FROM abc GROUP BY b ORDER BY x"
    );

    SQL_EQ(
        Select("a, b, c").from("abc").group_by("x").limit("10"),
        "SELECT a, b, c FROM abc GROUP BY x LIMIT 10"
    );
}

TEST(sql, constructing_sqls_SelectOrderBy) {
    SQL_EQ(
        Select("a, b, c").from("abc").order_by("x").limit("10"),
        "SELECT a, b, c FROM abc ORDER BY x LIMIT 10"
    );
}

TEST(sql, constructing_sqls_Update) { SQL_EQ(Update("abc").set("a=42"), "UPDATE abc SET a=42"); }

TEST(sql, constructing_sqls_UpdateSet) {
    SQL_EQ(Update("abc").set("a=42").where("b=7"), "UPDATE abc SET a=42 WHERE b=7");
}

TEST(sql, constructing_sqls_DeleteFrom) {
    SQL_EQ(DeleteFrom("abc"), "DELETE FROM abc");

    SQL_EQ(DeleteFrom("abc").where("b=7"), "DELETE FROM abc WHERE b=7");
}

TEST(sql, constructing_sqls_InsertInto) {
    SQL_EQ(
        InsertInto("abc(a, b, c)").values("?, ?, 1", false, 42),
        "INSERT INTO abc(a, b, c) VALUES(?, ?, 1)"
    );
}

TEST(sql, constructing_sqls_InsertSelect_InsertSelectFrom) {
    SQL_EQ(
        InsertInto("abc(a, b, c)").select("?, x, y", 1).from("xyz"),
        "INSERT INTO abc(a, b, c) SELECT ?, x, y FROM xyz"
    );

    SQL_EQ(
        InsertInto("abc(a, b, c)").select("?, x, y", 1).from("xyz").where("x=42"),
        "INSERT INTO abc(a, b, c) SELECT ?, x, y FROM xyz WHERE x=42"
    );

    SQL_EQ(
        InsertInto("abc(a, b, c)").select("?, x, y", 1).from("xyz").group_by("x"),
        "INSERT INTO abc(a, b, c) SELECT ?, x, y FROM xyz GROUP BY x"
    );

    SQL_EQ(
        InsertInto("abc(a, b, c)").select("?, x, y", 1).from("xyz").order_by("y"),
        "INSERT INTO abc(a, b, c) SELECT ?, x, y FROM xyz ORDER BY y"
    );
}

TEST(sql, constructing_sqls_InsertSelectWhere) {
    SQL_EQ(
        InsertInto("abc(a, b, c)").select("?, x, y", 1).from("xyz").where("x=42").group_by("y"),
        "INSERT INTO abc(a, b, c) SELECT ?, x, y FROM xyz WHERE x=42 GROUP BY y"
    );

    SQL_EQ(
        InsertInto("abc(a, b, c)").select("?, x, y", 1).from("xyz").where("x=42").order_by("y"),
        "INSERT INTO abc(a, b, c) SELECT ?, x, y FROM xyz WHERE x=42 ORDER BY y"
    );
}

TEST(sql, constructing_sqls_InsertSelectGroupBy) {
    SQL_EQ(
        InsertInto("abc(a, b, c)").select("?, x, y", 1).from("xyz").group_by("x").order_by("y"),
        "INSERT INTO abc(a, b, c) SELECT ?, x, y FROM xyz GROUP BY x ORDER BY y"
    );
}

TEST(sql, constructing_sqls_InsertIgnoreInto) {
    SQL_EQ(
        InsertIgnoreInto("abc(a, b, c)").values("?, ?, 1", false, 42),
        "INSERT IGNORE INTO abc(a, b, c) VALUES(?, ?, 1)"
    );

    SQL_EQ(
        InsertIgnoreInto("abc(a, b, c)")
            .select("?, x, y", 1)
            .from("xyz")
            .group_by("x")
            .order_by("y"),
        "INSERT IGNORE INTO abc(a, b, c) SELECT ?, x, y FROM xyz GROUP BY x ORDER BY y"
    );
}

TEST(sql, counting_parameters_SqlWithParams) {
    EXPECT_THROW(SqlWithParams("?"), std::runtime_error);
}

TEST(sql, counting_parameters_Select) {
    EXPECT_THROW(Select("?"), std::runtime_error);
    EXPECT_THROW(Select("a").from("?"), std::runtime_error);
}

TEST(sql, counting_parameters_SelectFrom) {
    EXPECT_THROW(Select("a").from("b").left_join("?"), std::runtime_error);
    EXPECT_THROW(Select("a").from("b").left_join(Select("c").from("d"), "?"), std::runtime_error);
    EXPECT_THROW(
        Select("a").from("b").left_join(Select("c").from("d").where("e"), "?"), std::runtime_error
    );
    EXPECT_THROW(
        Select("a").from("b").left_join(Select("c").from("d").group_by("e"), "?"),
        std::runtime_error
    );
    EXPECT_THROW(
        Select("a").from("b").left_join(Select("c").from("d").order_by("e"), "?"),
        std::runtime_error
    );
    EXPECT_THROW(
        Select("a").from("b").left_join(Select("c").from("d").limit("e"), "?"), std::runtime_error
    );
    EXPECT_THROW(Select("a").from("b").right_join("?"), std::runtime_error);
    EXPECT_THROW(Select("a").from("b").right_join(Select("c").from("d"), "?"), std::runtime_error);
    EXPECT_THROW(
        Select("a").from("b").right_join(Select("c").from("d").where("e"), "?"), std::runtime_error
    );
    EXPECT_THROW(
        Select("a").from("b").right_join(Select("c").from("d").group_by("e"), "?"),
        std::runtime_error
    );
    EXPECT_THROW(
        Select("a").from("b").right_join(Select("c").from("d").order_by("e"), "?"),
        std::runtime_error
    );
    EXPECT_THROW(
        Select("a").from("b").right_join(Select("c").from("d").limit("e"), "?"), std::runtime_error
    );
    EXPECT_THROW(Select("a").from("b").inner_join("?"), std::runtime_error);
    EXPECT_THROW(Select("a").from("b").inner_join(Select("c").from("d"), "?"), std::runtime_error);
    EXPECT_THROW(
        Select("a").from("b").inner_join(Select("c").from("d").where("e"), "?"), std::runtime_error
    );
    EXPECT_THROW(
        Select("a").from("b").inner_join(Select("c").from("d").group_by("e"), "?"),
        std::runtime_error
    );
    EXPECT_THROW(
        Select("a").from("b").inner_join(Select("c").from("d").order_by("e"), "?"),
        std::runtime_error
    );
    EXPECT_THROW(
        Select("a").from("b").inner_join(Select("c").from("d").limit("e"), "?"), std::runtime_error
    );
    EXPECT_THROW(Select("a").from("b").where("?"), std::runtime_error);
    EXPECT_THROW(Select("a").from("b").group_by("?"), std::runtime_error);
    EXPECT_THROW(Select("a").from("b").order_by("?"), std::runtime_error);
    EXPECT_THROW(Select("a").from("b").limit("?"), std::runtime_error);
}

TEST(sql, counting_parameters_SelectJoin) {
    EXPECT_THROW(Select("a").from("b").inner_join("c").on("?"), std::runtime_error);
}

TEST(sql, counting_parameters_SelectJoinOn) {
    EXPECT_THROW(Select("a").from("b").inner_join("c").on("d").left_join("?"), std::runtime_error);
    EXPECT_THROW(
        Select("a").from("b").inner_join("c").on("d").left_join(Select("e").from("f"), "?"),
        std::runtime_error
    );
    EXPECT_THROW(
        Select("a").from("b").inner_join("c").on("d").left_join(
            Select("e").from("f").where("g"), "?"
        ),
        std::runtime_error
    );
    EXPECT_THROW(
        Select("a").from("b").inner_join("c").on("d").left_join(
            Select("e").from("f").group_by("g"), "?"
        ),
        std::runtime_error
    );
    EXPECT_THROW(
        Select("a").from("b").inner_join("c").on("d").left_join(
            Select("e").from("f").order_by("g"), "?"
        ),
        std::runtime_error
    );
    EXPECT_THROW(
        Select("a").from("b").inner_join("c").on("d").left_join(
            Select("e").from("f").limit("g"), "?"
        ),
        std::runtime_error
    );
    EXPECT_THROW(Select("a").from("b").inner_join("c").on("d").right_join("?"), std::runtime_error);
    EXPECT_THROW(
        Select("a").from("b").inner_join("c").on("d").right_join(Select("e").from("f"), "?"),
        std::runtime_error
    );
    EXPECT_THROW(
        Select("a").from("b").inner_join("c").on("d").right_join(
            Select("e").from("f").where("g"), "?"
        ),
        std::runtime_error
    );
    EXPECT_THROW(
        Select("a").from("b").inner_join("c").on("d").right_join(
            Select("e").from("f").group_by("g"), "?"
        ),
        std::runtime_error
    );
    EXPECT_THROW(
        Select("a").from("b").inner_join("c").on("d").right_join(
            Select("e").from("f").order_by("g"), "?"
        ),
        std::runtime_error
    );
    EXPECT_THROW(
        Select("a").from("b").inner_join("c").on("d").right_join(
            Select("e").from("f").limit("g"), "?"
        ),
        std::runtime_error
    );
    EXPECT_THROW(Select("a").from("b").inner_join("c").on("d").inner_join("?"), std::runtime_error);
    EXPECT_THROW(
        Select("a").from("b").inner_join("c").on("d").inner_join(Select("e").from("f"), "?"),
        std::runtime_error
    );
    EXPECT_THROW(
        Select("a").from("b").inner_join("c").on("d").inner_join(
            Select("e").from("f").where("g"), "?"
        ),
        std::runtime_error
    );
    EXPECT_THROW(
        Select("a").from("b").inner_join("c").on("d").inner_join(
            Select("e").from("f").group_by("g"), "?"
        ),
        std::runtime_error
    );
    EXPECT_THROW(
        Select("a").from("b").inner_join("c").on("d").inner_join(
            Select("e").from("f").order_by("g"), "?"
        ),
        std::runtime_error
    );
    EXPECT_THROW(
        Select("a").from("b").inner_join("c").on("d").inner_join(
            Select("e").from("f").limit("g"), "?"
        ),
        std::runtime_error
    );
    EXPECT_THROW(Select("a").from("b").inner_join("c").on("d").where("?"), std::runtime_error);
    EXPECT_THROW(Select("a").from("b").inner_join("c").on("d").group_by("?"), std::runtime_error);
    EXPECT_THROW(Select("a").from("b").inner_join("c").on("d").order_by("?"), std::runtime_error);
    EXPECT_THROW(Select("a").from("b").inner_join("c").on("d").limit("?"), std::runtime_error);
}

TEST(sql, counting_parameters_SelectWhere) {
    EXPECT_THROW(Select("a").from("b").where("c").group_by("?"), std::runtime_error);
    EXPECT_THROW(Select("a").from("b").where("c").order_by("?"), std::runtime_error);
    EXPECT_THROW(Select("a").from("b").where("c").limit("?"), std::runtime_error);
}

TEST(sql, counting_parameters_SelectGroupBy) {
    EXPECT_THROW(Select("a").from("b").group_by("c").order_by("?"), std::runtime_error);
    EXPECT_THROW(Select("a").from("b").group_by("c").limit("?"), std::runtime_error);
}

TEST(sql, counting_parameters_SelectOrderBy) {
    EXPECT_THROW(Select("a").from("b").order_by("c").limit("?"), std::runtime_error);
}

TEST(sql, counting_parameters_Condition) { EXPECT_THROW(Condition("?"), std::runtime_error); }

TEST(sql, counting_parameters_Update) {
    EXPECT_THROW(Update("?"), std::runtime_error);
    EXPECT_THROW(Update("a").set("?"), std::runtime_error);
}

TEST(sql, counting_parameters_UpdateSet) {
    EXPECT_THROW(Update("a").set("c=1").where("?"), std::runtime_error);
}

TEST(sql, counting_parameters_DeleteFrom) {
    EXPECT_THROW(DeleteFrom("?"), std::runtime_error);
    EXPECT_THROW(DeleteFrom("a").where("?"), std::runtime_error);
}

TEST(sql, counting_parameters_InsertInto) {
    EXPECT_THROW(InsertInto("?"), std::runtime_error);
    EXPECT_THROW(InsertInto("a").values("?"), std::runtime_error);
    EXPECT_THROW(InsertInto("a").select("?"), std::runtime_error);
}

TEST(sql, counting_parameters_InsertIgnoreInto) {
    EXPECT_THROW(InsertIgnoreInto("?"), std::runtime_error);
    EXPECT_THROW(InsertIgnoreInto("a").values("?"), std::runtime_error);
    EXPECT_THROW(InsertIgnoreInto("a").select("?"), std::runtime_error);
}

TEST(sql, counting_parameters_InsertSelect) {
    EXPECT_THROW(InsertInto("a").select("b").from("?"), std::runtime_error);
}

TEST(sql, counting_parameters_InsertSelectFrom) {
    EXPECT_THROW(InsertInto("a").select("b").from("c").where("?"), std::runtime_error);
    EXPECT_THROW(InsertInto("a").select("b").from("c").group_by("?"), std::runtime_error);
    EXPECT_THROW(InsertInto("a").select("b").from("c").order_by("?"), std::runtime_error);
}

TEST(sql, counting_parameters_InsertSelectWhere) {
    EXPECT_THROW(
        InsertInto("a").select("b").from("c").where("d").group_by("?"), std::runtime_error
    );
    EXPECT_THROW(
        InsertInto("a").select("b").from("c").where("d").order_by("?"), std::runtime_error
    );
}

TEST(sql, counting_parameters_InsertSelectGroupBy) {
    EXPECT_THROW(
        InsertInto("a").select("b").from("c").group_by("d").order_by("?"), std::runtime_error
    );
}

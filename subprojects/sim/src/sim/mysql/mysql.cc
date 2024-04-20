#include <cassert>
#include <cstddef>
#include <cstdint>
#include <errmsg.h>
#include <exception>
#include <fcntl.h>
#include <mutex>
#include <mysql.h>
#include <optional>
#include <sim/mysql/mysql.hh>
#include <sim/sql/sql.hh>
#include <simlib/always_false.hh>
#include <simlib/config_file.hh>
#include <simlib/defer.hh>
#include <simlib/enum_to_underlying_type.hh>
#include <simlib/errmsg.hh>
#include <simlib/inplace_buff.hh>
#include <simlib/macros/enum_with_string_conversions.hh>
#include <simlib/macros/stack_unwinding.hh>
#include <simlib/macros/throw.hh>
#include <simlib/string_traits.hh>
#include <simlib/string_view.hh>
#include <simlib/throw_assert.hh>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace {

MYSQL* new_mysql() {
    STACK_UNWINDING_MARK;
    // Some documentations say that invoking mysql_init() is unsafe in a multi-thread environment
    static std::mutex mysql_init_mutex;
    MYSQL* conn = [] {
        auto guard = std::lock_guard{mysql_init_mutex};
        return mysql_init(nullptr);
    }();
    if (!conn) {
        THROW("mysql_init() failed");
    }
    return conn;
}

} // namespace

namespace sim::mysql {

Connection::Connection(
    const char* host, const char* user, const char* password, const char* database
)
: conn{new_mysql()} {
    try {
        STACK_UNWINDING_MARK;

        my_bool true_val = true;
        if (mysql_optionsv(conn, MYSQL_REPORT_DATA_TRUNCATION, &true_val)) {
            THROW(mysql_error(conn));
        }
        if (!mysql_real_connect(conn, host, user, password, database, 0, nullptr, 0)) {
            THROW(mysql_error(conn));
        }
        // Set CLOEXEC flag on the mysql connection socket
        int mysql_socket = mysql_get_socket(conn);
        int flags = fcntl(mysql_socket, F_GETFD);
        if (flags == -1) {
            THROW("fnctl()", errmsg());
        }
        if (fcntl(mysql_socket, F_SETFD, flags | FD_CLOEXEC)) {
            THROW("fnctl()", errmsg());
        }
    } catch (...) {
        mysql_close(conn);
        throw;
    }
}

Connection Connection::from_credential_file(const char* credential_file_path) {
    STACK_UNWINDING_MARK;

    ConfigFile cf;
    cf.add_vars("host", "user", "password", "db");
    cf.load_config_from_file(credential_file_path);
    for (const auto& it : cf.get_vars()) {
        if (!it.second.is_set()) {
            THROW(credential_file_path, ": variable `", it.first, "` is not set");
        }
        if (it.second.is_array()) {
            THROW(
                credential_file_path, ": variable `", it.first, "` cannot be specified as an array"
            );
        }
    }
    return Connection(
        cf["host"].as_string().c_str(),
        cf["user"].as_string().c_str(),
        cf["password"].as_string().c_str(),
        cf["db"].as_string().c_str()
    );
}

Connection::~Connection() {
    assert(referencing_objects_num == 0);
    mysql_close(conn);
}

Transaction Connection::start_serializable_transaction() {
    STACK_UNWINDING_MARK;
    update("SET TRANSACTION ISOLATION LEVEL SERIALIZABLE");
    update("START TRANSACTION");
    return Transaction{conn, &referencing_objects_num};
}

Transaction Connection::start_repeatable_read_transaction() {
    STACK_UNWINDING_MARK;
    update("SET TRANSACTION ISOLATION LEVEL REPEATABLE READ");
    update("START TRANSACTION");
    return Transaction{conn, &referencing_objects_num};
}

Transaction Connection::start_read_committed_transaction() {
    STACK_UNWINDING_MARK;
    update("SET TRANSACTION ISOLATION LEVEL READ COMMITTED");
    update("START TRANSACTION");
    return Transaction{conn, &referencing_objects_num};
}

void Connection::update(std::string_view sql) {
    STACK_UNWINDING_MARK;

    bool retrying = false;
    for (;;) {
        if (mysql_real_query(conn, sql.data(), sql.size())) {
            bool safe_to_reconnect = referencing_objects_num == 0;
            if (!retrying && mysql_errno(conn) == CR_SERVER_GONE_ERROR && safe_to_reconnect) {
                // mysql_real_connect() may be called only once on a connection -- a new one must be
                // created.
                auto new_connection = Connection(conn->host, conn->user, conn->passwd, conn->db);
                std::swap(conn, new_connection.conn);
                retrying = true;
                continue;
            }
            THROW(mysql_error(conn));
        }
        break;
    }
}

Transaction::Transaction(MYSQL* conn, size_t* connection_referencing_objects_num) noexcept
: conn{conn}
, connection_referencing_objects_num{connection_referencing_objects_num} {
    ++*connection_referencing_objects_num;
}

Transaction::~Transaction() noexcept(false) {
    if (conn) {
        --*connection_referencing_objects_num;
        if (mysql_rollback(conn) && uncaught_exceptions == std::uncaught_exceptions()) {
            THROW(mysql_error(conn));
        }
    }
}

void Transaction::commit() {
    STACK_UNWINDING_MARK;

    if (mysql_commit(conn)) {
        THROW(mysql_error(conn));
    }
    --*connection_referencing_objects_num;
    conn = nullptr;
}

void Transaction::rollback() {
    STACK_UNWINDING_MARK;

    if (mysql_rollback(conn)) {
        THROW(mysql_error(conn));
    }
    --*connection_referencing_objects_num;
    conn = nullptr;
}

Statement::Statement(
    MYSQL* conn, size_t* connection_referencing_objects_num, MYSQL_STMT* stmt
) noexcept
: conn{conn}
, connection_referencing_objects_num{connection_referencing_objects_num}
, stmt{stmt} {
    ++*connection_referencing_objects_num;
}

Statement::~Statement() noexcept(false) {
    --*connection_referencing_objects_num;
    if (stmt && mysql_stmt_close(stmt) && uncaught_exceptions == std::uncaught_exceptions()) {
        THROW(mysql_error(conn));
    }
}

Statement::Statement(Statement&& other) noexcept
: conn{other.conn}
, connection_referencing_objects_num{other.connection_referencing_objects_num}
, stmt{std::exchange(other.stmt, nullptr)}
, field_count{other.field_count}
, res{std::move(other.res)} {
    ++*connection_referencing_objects_num;
}

Statement& Statement::operator=(Statement&& other) noexcept {
    if (this != &other) {
        conn = other.conn;
        std::swap(connection_referencing_objects_num, other.connection_referencing_objects_num);
        stmt = std::exchange(other.stmt, nullptr);
        field_count = other.field_count;
        res = std::move(other.res);
    }
    return *this;
}

void Statement::do_not_store_result() noexcept { store_result = false; }

bool Statement::next() {
    STACK_UNWINDING_MARK;
    // This is needed if we changed the bind parameters.
    if (res.previous_fetch_changed_binds && mysql_stmt_bind_result(stmt, res.mysql_binds.get())) {
        THROW(mysql_stmt_error(stmt));
    }
    res.previous_fetch_changed_binds = false;

    auto rc = mysql_stmt_fetch(stmt);
    if (rc == MYSQL_NO_DATA) {
        return false;
    }
    if (rc != 0 && rc != MYSQL_DATA_TRUNCATED) {
        THROW(mysql_stmt_error(stmt));
    }
    if (!res.binds) {
        return true; // Don't proceed if res_bind() was not called.
    }

    for (size_t column = 0; column < field_count; ++column) {
        auto& mysql_bind = res.mysql_binds[column];
        bool truncated = false;
        if (mysql_bind.error_value) {
            if (rc == MYSQL_DATA_TRUNCATED) {
                truncated = true;
            } else {
                THROW("Unexpected error at column: ", column);
            }
        }

        std::visit(
            [&](auto& bind) {
                using Kind = std::remove_reference_t<decltype(bind)>;
                if constexpr (std::is_same_v<Kind, Res::Bind::Bool>) {
                    if (mysql_bind.is_null_value) {
                        THROW(
                            "Unexpected NULL at column: ",
                            column,
                            " (bool& does not accept NULL values)"
                        );
                    }
                    if (truncated || bind.bind_value > 1) {
                        THROW("Truncated data at column: ", column);
                    }
                    *bind.dest = static_cast<bool>(bind.bind_value);
                } else if constexpr (std::is_same_v<Kind, Res::Bind::Numeric>) {
                    if (mysql_bind.is_null_value) {
                        THROW(
                            "Unexpected NULL at column: ",
                            column,
                            " (bound reference does not accept NULL values)"
                        );
                    }
                    if (truncated) {
                        THROW("Truncated data at column: ", column);
                    }
                } else if constexpr (std::is_same_v<Kind, Res::Bind::String>) {
                    if (mysql_bind.is_null_value) {
                        THROW(
                            "Unexpected NULL at column: ",
                            column,
                            " (std::string& does not accept NULL values)"
                        );
                    }
                    auto& inplace_buff = bind.buff;
                    if (truncated) {
                        res.previous_fetch_changed_binds =
                            true; // Needs to happen before a potential exception.
                        try {
                            inplace_buff.lossy_resize(inplace_buff.size);
                            mysql_bind.buffer = inplace_buff.data();
                            mysql_bind.buffer_length = inplace_buff.max_size();
                        } catch (...) {
                            mysql_bind.buffer = inplace_buff.data();
                            mysql_bind.buffer_length = inplace_buff.max_size();
                            throw;
                        }
                        if (mysql_stmt_fetch_column(stmt, &mysql_bind, column, 0)) {
                            THROW(mysql_stmt_error(stmt));
                        }
                    }
                    bind.str->assign(inplace_buff.data(), inplace_buff.size);
                } else if constexpr (std::is_same_v<Kind, Res::Bind::InplaceBuff>) {
                    if (mysql_bind.is_null_value) {
                        THROW(
                            "Unexpected NULL at column: ",
                            column,
                            " (InplaceBuff& does not accept NULL values)"
                        );
                    }
                    auto& inplace_buff = *bind.inplace_buff;
                    if (truncated) {
                        res.previous_fetch_changed_binds =
                            true; // Needs to happen before a potential exception.
                        try {
                            inplace_buff.lossy_resize(inplace_buff.size);
                            mysql_bind.buffer = inplace_buff.data();
                            mysql_bind.buffer_length = inplace_buff.max_size();
                        } catch (...) {
                            mysql_bind.buffer = inplace_buff.data();
                            mysql_bind.buffer_length = inplace_buff.max_size();
                            throw;
                        }
                        if (mysql_stmt_fetch_column(stmt, &mysql_bind, column, 0)) {
                            THROW(mysql_stmt_error(stmt));
                        }
                    }
                } else if constexpr (Res::Bind::IsEnumWithStringConversions<Kind>::value) {
                    if (mysql_bind.is_null_value) {
                        THROW(
                            "Unexpected NULL at column: ",
                            column,
                            " (bound reference does not accept NULL values)"
                        );
                    }
                    if (truncated) {
                        THROW("Truncated data at column: ", column);
                    }
                    bind.callback(column, bind.bind_value, bind.dest);
                } else if constexpr (std::is_same_v<Kind, Res::Bind::OptionalBool>) {
                    if (mysql_bind.is_null_value) {
                        *bind.dest = std::nullopt;
                    } else {
                        if (truncated) {
                            THROW("Truncated data at column: ", column);
                        }
                        *bind.dest = static_cast<bool>(bind.value);
                    }
                } else if constexpr (Res::Bind::IsOptionalNumeric<Kind>::value) {
                    if (mysql_bind.is_null_value) {
                        *bind.dest = std::nullopt;
                    } else {
                        if (truncated) {
                            THROW("Truncated data at column: ", column);
                        }
                        *bind.dest = bind.value;
                    }
                } else if constexpr (std::is_same_v<Kind, Res::Bind::OptionalStringlike>) {
                    if (mysql_bind.is_null_value) {
                        bind.callback(nullptr, bind.dest);
                    } else {
                        auto& inplace_buff = bind.buff;
                        if (truncated) {
                            res.previous_fetch_changed_binds =
                                true; // Needs to happen before a potential exception.
                            try {
                                inplace_buff.lossy_resize(inplace_buff.size);
                                mysql_bind.buffer = inplace_buff.data();
                                mysql_bind.buffer_length = inplace_buff.max_size();
                            } catch (...) {
                                mysql_bind.buffer = inplace_buff.data();
                                mysql_bind.buffer_length = inplace_buff.max_size();
                                throw;
                            }
                            if (mysql_stmt_fetch_column(stmt, &mysql_bind, column, 0)) {
                                THROW(mysql_stmt_error(stmt));
                            }
                        }
                        bind.callback(&inplace_buff, bind.dest);
                    }
                } else if constexpr (Res::Bind::IsOptionalEnumWithStringConversions<Kind>::value) {
                    if (truncated) {
                        THROW("Truncated data at column: ", column);
                    }
                    bind.callback(
                        column,
                        mysql_bind.is_null_value ? std::nullopt : std::optional{bind.bind_value},
                        bind.dest
                    );
                } else {
                    static_assert(always_false<Kind>, "BUG: unhandled type");
                }
            },
            res.binds[column].kind
        );
    }
    return true;
}

uint64_t Statement::affected_rows() noexcept { return mysql_stmt_affected_rows(stmt); }

uint64_t Statement::insert_id() noexcept { return mysql_stmt_insert_id(stmt); }

} // namespace sim::mysql

/********************************************* TESTS *********************************************/

#define assert_throws(code, description)                   \
    try {                                                  \
        code;                                              \
        THROW(#code " did not throw");                     \
    } catch (const std::exception& e) {                    \
        throw_assert(has_prefix(e.what(), (description))); \
    }

namespace {

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#endif
ENUM_WITH_STRING_CONVERSIONS(EWSC, uint8_t,
    (A, 0, "aa")
    (B, 7, "bb")
    (C, 42, "cc")
);
#ifdef __clang__
#pragma clang diagnostic pop
#endif

} // namespace

namespace sim::mysql {

void run_unit_tests(sim::mysql::Connection& mysql) {
    STACK_UNWINDING_MARK;

    using sim::sql::InsertInto;
    using sim::sql::Select;
    using sim::sql::SqlWithParams;
    // nullptr and std::nullopt
    {
        mysql.execute(sim::sql::SqlWithParams(
            "CREATE TABLE test(id bigint unsigned NOT NULL, x int DEFAULT NULL, PRIMARY KEY(id))"
        ));
        auto dropper = Defer{[&] { mysql.execute(SqlWithParams("DROP TABLE test")); }};
        mysql.execute(InsertInto("test(id, x)").values("1, ?", nullptr));
        mysql.execute(InsertInto("test(id, x)").values("2, ?", std::nullopt));

        // optional<int>
        auto stmt = mysql.execute(Select("x").from("test").order_by("id"));
        std::optional<int> oi;
        stmt.res_bind(oi);
        throw_assert(stmt.next() && oi == std::nullopt);
        throw_assert(stmt.next() && oi == std::nullopt);
        throw_assert(!stmt.next());
    }
    // bool and optional<bool>
    {
        mysql.execute(
            SqlWithParams("CREATE TABLE test(id bigint unsigned NOT NULL, x tinyint DEFAULT NULL, "
                          "PRIMARY KEY(id))")
        );
        auto dropper = Defer{[&] { mysql.execute(SqlWithParams("DROP TABLE test")); }};
        mysql.execute(InsertInto("test(id, x)").values("1, ?", true));
        mysql.execute(InsertInto("test(id, x)").values("2, ?", nullptr));
        mysql.execute(InsertInto("test(id, x)").values("3, ?", std::optional{false}));

        // bool
        auto stmt = mysql.execute(Select("x").from("test").order_by("id"));
        bool b;
        stmt.res_bind(b);
        throw_assert(stmt.next() && b == true);
        assert_throws(
            stmt.next(), "Unexpected NULL at column: 0 (bool& does not accept NULL values)"
        );
        throw_assert(stmt.next() && b == false);
        throw_assert(!stmt.next());

        // optional<bool>
        stmt = mysql.execute(Select("x").from("test").order_by("id"));
        std::optional<bool> ob;
        stmt.res_bind(ob);
        throw_assert(stmt.next() && ob == true);
        throw_assert(stmt.next() && ob == std::nullopt);
        throw_assert(stmt.next() && ob == false);
        throw_assert(!stmt.next());

        // Truncation
        stmt = mysql.execute(SqlWithParams("SELECT 2"));
        stmt.res_bind(b);
        assert_throws(stmt.next(), "Truncated data at column: 0");
    }
    // signed short and optional<signed short>
    {
        mysql.execute(
            SqlWithParams("CREATE TABLE test(id bigint unsigned NOT NULL, x smallint DEFAULT NULL, "
                          "PRIMARY KEY(id))")
        );
        auto dropper = Defer{[&] { mysql.execute(SqlWithParams("DROP TABLE test")); }};
        mysql.execute(InsertInto("test(id, x)").values("1, ?", -1));
        mysql.execute(InsertInto("test(id, x)").values("2, ?", nullptr));
        mysql.execute(InsertInto("test(id, x)").values("3, ?", std::optional{-2}));

        // signed short
        auto stmt = mysql.execute(Select("x").from("test").order_by("id"));
        int16_t i;
        stmt.res_bind(i);
        throw_assert(stmt.next() && i == -1);
        assert_throws(
            stmt.next(),
            "Unexpected NULL at column: 0 (bound reference does not accept NULL values)"
        );
        throw_assert(stmt.next() && i == -2);
        throw_assert(!stmt.next());

        // optional<signed short>
        stmt = mysql.execute(Select("x").from("test").order_by("id"));
        std::optional<int16_t> oi;
        stmt.res_bind(oi);
        throw_assert(stmt.next() && oi == -1);
        throw_assert(stmt.next() && oi == std::nullopt);
        throw_assert(stmt.next() && oi == -2);
        throw_assert(!stmt.next());

        // Truncation
        stmt = mysql.execute(SqlWithParams("SELECT 1000000"));
        stmt.res_bind(i);
        assert_throws(stmt.next(), "Truncated data at column: 0");
    }
    // unsigned short and optional<unsigned short>
    {
        mysql.execute(SqlWithParams(
            "CREATE TABLE test(id bigint unsigned NOT NULL, x smallint unsigned DEFAULT "
            "NULL, PRIMARY KEY(id))"
        ));
        auto dropper = Defer{[&] { mysql.execute(SqlWithParams("DROP TABLE test")); }};
        mysql.execute(InsertInto("test(id, x)").values("1, ?", 7));
        mysql.execute(InsertInto("test(id, x)").values("2, ?", nullptr));
        mysql.execute(InsertInto("test(id, x)").values("3, ?", std::optional{42}));

        // unsigned short
        auto stmt = mysql.execute(Select("x").from("test").order_by("id"));
        uint16_t i;
        stmt.res_bind(i);
        throw_assert(stmt.next() && i == 7);
        assert_throws(
            stmt.next(),
            "Unexpected NULL at column: 0 (bound reference does not accept NULL values)"
        );
        throw_assert(stmt.next() && i == 42);
        throw_assert(!stmt.next());

        // optional<unsigned short>
        stmt = mysql.execute(Select("x").from("test").order_by("id"));
        std::optional<uint16_t> oi;
        stmt.res_bind(oi);
        throw_assert(stmt.next() && oi == 7);
        throw_assert(stmt.next() && oi == std::nullopt);
        throw_assert(stmt.next() && oi == 42);
        throw_assert(!stmt.next());

        // Truncation
        stmt = mysql.execute(SqlWithParams("SELECT -1"));
        stmt.res_bind(i);
        assert_throws(stmt.next(), "Truncated data at column: 0");
    }
    // std::string and optional<std::string>
    {
        mysql.execute(SqlWithParams(
            "CREATE TABLE test(id bigint unsigned NOT NULL, x VARBINARY(10) DEFAULT NULL, "
            "PRIMARY KEY(id))"
        ));
        auto dropper = Defer{[&] { mysql.execute(SqlWithParams("DROP TABLE test")); }};
        mysql.execute(InsertInto("test(id, x)").values("1, ?", "aaa"));
        mysql.execute(InsertInto("test(id, x)").values("2, ?", nullptr));
        mysql.execute(InsertInto("test(id, x)").values("3, ?", std::string{"bbb"}));
        mysql.execute(InsertInto("test(id, x)").values("4, ?", std::string_view{"ccc ddd"}));
        mysql.execute(InsertInto("test(id, x)").values("5, ?", StringView{"eee"}));
        mysql.execute(InsertInto("test(id, x)").values("6, ?", std::optional{InplaceBuff<3>{""}}));
        mysql.execute(InsertInto("test(id, x)").values("7, ?", std::optional{std::string{"xxx"}}));

        // std::string
        auto stmt = mysql.execute(Select("x").from("test").order_by("id"));
        std::string s;
        stmt.res_bind(s);
        throw_assert(stmt.next() && s == "aaa");
        assert_throws(
            stmt.next(), "Unexpected NULL at column: 0 (std::string& does not accept NULL values)"
        );
        throw_assert(stmt.next() && s == "bbb");
        throw_assert(stmt.next() && s == "ccc ddd");
        throw_assert(stmt.next() && s == "eee");
        throw_assert(stmt.next() && s == "");
        throw_assert(stmt.next() && s == "xxx");
        throw_assert(!stmt.next());

        // optional<std::string>
        stmt = mysql.execute(Select("x").from("test").order_by("id"));
        std::optional<std::string> os;
        stmt.res_bind(os);
        throw_assert(stmt.next() && os == "aaa");
        throw_assert(stmt.next() && os == std::nullopt);
        throw_assert(stmt.next() && os == "bbb");
        throw_assert(stmt.next() && os == "ccc ddd");
        throw_assert(stmt.next() && os == "eee");
        throw_assert(stmt.next() && os == "");
        throw_assert(stmt.next() && os == "xxx");
        throw_assert(!stmt.next());
    }
    // InplaceBuff
    {
        mysql.execute(SqlWithParams(
            "CREATE TABLE test(id bigint unsigned NOT NULL, x VARBINARY(10) DEFAULT NULL, "
            "PRIMARY KEY(id))"
        ));
        auto dropper = Defer{[&] { mysql.execute(SqlWithParams("DROP TABLE test")); }};
        mysql.execute(InsertInto("test(id, x)").values("1, ?", "aaa"));
        mysql.execute(InsertInto("test(id, x)").values("2, ?", nullptr));
        mysql.execute(InsertInto("test(id, x)").values("3, ?", std::string{"bbb"}));
        mysql.execute(InsertInto("test(id, x)").values("4, ?", std::string_view{"ccc ddd"}));
        mysql.execute(InsertInto("test(id, x)").values("5, ?", StringView{"eee"}));
        mysql.execute(InsertInto("test(id, x)").values("6, ?", std::optional{InplaceBuff<3>{""}}));
        mysql.execute(InsertInto("test(id, x)").values("7, ?", std::optional{std::string{"xxx"}}));

        // InplaceBuff
        auto stmt = mysql.execute(Select("x").from("test").order_by("id"));
        InplaceBuff<3> s;
        stmt.res_bind(s);
        throw_assert(stmt.next() && s == "aaa");
        assert_throws(
            stmt.next(), "Unexpected NULL at column: 0 (InplaceBuff& does not accept NULL values)"
        );
        throw_assert(stmt.next() && s == "bbb");
        throw_assert(stmt.next() && s == "ccc ddd");
        throw_assert(stmt.next() && s == "eee");
        throw_assert(stmt.next() && s == "");
        throw_assert(stmt.next() && s == "xxx");
        throw_assert(!stmt.next());
    }
    // Binding Enum and optional<Enum>
    {
        enum class Enum : int {
            A = 0,
            B = 37,
            C = 6'123'562,
        };

        mysql.execute(SqlWithParams(
            "CREATE TABLE test(id bigint unsigned NOT NULL, x int unsigned DEFAULT NULL, "
            "PRIMARY KEY(id))"
        ));
        auto dropper = Defer{[&] { mysql.execute(SqlWithParams("DROP TABLE test")); }};
        mysql.execute(InsertInto("test(id, x)").values("1, ?", Enum::A));
        mysql.execute(InsertInto("test(id, x)").values("2, ?", Enum::B));
        mysql.execute(InsertInto("test(id, x)").values("3, ?", std::optional<Enum>{std::nullopt}));
        mysql.execute(InsertInto("test(id, x)").values("4, ?", std::optional{Enum::C}));

        std::optional<int> x;
        auto stmt = mysql.execute(Select("x").from("test").order_by("id"));
        stmt.res_bind(x);
        throw_assert(stmt.next() && x == enum_to_underlying_type(Enum::A));
        throw_assert(stmt.next() && x == enum_to_underlying_type(Enum::B));
        throw_assert(stmt.next() && x == std::nullopt);
        throw_assert(stmt.next() && x == enum_to_underlying_type(Enum::C));
        throw_assert(!stmt.next());
    }
    // ENUM_WITH_STRING_CONVERSIONS
    {
        mysql.execute(SqlWithParams(
            "CREATE TABLE test(id bigint unsigned NOT NULL, x tinyint unsigned DEFAULT NULL, "
            "PRIMARY KEY(id))"
        ));
        auto dropper = Defer{[&] { mysql.execute(SqlWithParams("DROP TABLE test")); }};
        mysql.execute(InsertInto("test(id, x)").values("1, ?", EWSC::A)); // enum
        mysql.execute(InsertInto("test(id, x)").values("2, ?", EWSC{EWSC::B})); // EWSC
        mysql.execute(InsertInto("test(id, x)").values("3, ?", std::optional<EWSC>{std::nullopt}));
        mysql.execute(InsertInto("test(id, x)").values("4, ?", std::optional<EWSC>{EWSC::C}));

        // ENUM_WITH_STRING_CONVERSIONS
        auto stmt = mysql.execute(Select("x").from("test").order_by("id"));
        EWSC e;
        stmt.res_bind(e);
        throw_assert(stmt.next() && e == EWSC::A);
        throw_assert(stmt.next() && e == EWSC::B);
        assert_throws(
            stmt.next(),
            "Unexpected NULL at column: 0 (bound reference does not accept NULL values)"
        );
        throw_assert(stmt.next() && e == EWSC::C);
        throw_assert(!stmt.next());
        // Unmapped value
        stmt = mysql.execute(SqlWithParams("SELECT 3"));
        stmt.res_bind(e);
        assert_throws(stmt.next(), "Invalid value for ENUM_WITH_STRING_CONVERSIONS at column: 0");

        // optional<ENUM_WITH_STRING_CONVERSIONS>
        stmt = mysql.execute(Select("x").from("test").order_by("id"));
        std::optional<EWSC> oe;
        stmt.res_bind(oe);
        throw_assert(stmt.next() && oe == EWSC::A);
        throw_assert(stmt.next() && oe == EWSC::B);
        throw_assert(stmt.next() && oe == std::nullopt);
        throw_assert(stmt.next() && oe == EWSC::C);
        // Unmapped value
        stmt = mysql.execute(SqlWithParams("SELECT 3"));
        stmt.res_bind(oe);
        assert_throws(stmt.next(), "Invalid value for ENUM_WITH_STRING_CONVERSIONS at column: 0");

        // Truncation
        stmt = mysql.execute(SqlWithParams("SELECT -1"));
        stmt.res_bind(e);
        assert_throws(stmt.next(), "Truncated data at column: 0");
    }
    // Errors
    assert_throws(mysql.update("ABC"), "You have an error in your SQL syntax");
    assert_throws(mysql.execute(SqlWithParams("ABC")), "You have an error in your SQL syntax");
}

} // namespace sim::mysql

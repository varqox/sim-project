#pragma once

#include <cassert>
#include <cstddef>
#include <cstring>
#include <errmsg.h>
#include <exception>
#include <memory>
#include <mysql.h>
#include <optional>
#include <sim/sql/sql.hh>
#include <simlib/always_false.hh>
#include <simlib/inplace_buff.hh>
#include <simlib/macros/enum_with_string_conversions.hh>
#include <simlib/macros/stack_unwinding.hh>
#include <simlib/macros/throw.hh>
#include <simlib/meta/is_one_of.hh>
#include <simlib/string_view.hh>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace old_mysql {
class ConnectionView;
} // namespace old_mysql

namespace sim::mysql {

class Transaction;
class Statement;

class Connection {
    MYSQL* conn;
    size_t referencing_objects_num = 0;

    friend class ::old_mysql::ConnectionView; // TODO: remove after removing old_mysql

public:
    explicit Connection(
        const char* host, const char* user, const char* password, const char* database
    );

    static Connection from_credential_file(const char* credential_file_path);

    ~Connection();

    Connection(const Connection&) = delete;
    Connection(Connection&&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection& operator=(Connection&&) = delete;

    Transaction start_serializable_transaction();

    Transaction start_repeatable_read_transaction();

    Transaction start_read_committed_transaction();

    void update(std::string_view sql);

    template <class SqlExpr, class = decltype(sql::SqlWithParams{std::declval<SqlExpr&&>()})>
    Statement execute(SqlExpr&& sql_expr);
};

class Transaction {
    friend class Connection;

    MYSQL* conn;
    size_t* connection_referencing_objects_num;
    int uncaught_exceptions = std::uncaught_exceptions();

    explicit Transaction(MYSQL* conn, size_t* connection_referencing_objects_num) noexcept;

public:
    ~Transaction() noexcept(false);

    Transaction(const Transaction&) = delete;
    Transaction(Transaction&&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    Transaction& operator=(Transaction&&) = delete;

    void commit();

    void rollback();
};

// Cannot outlive Connection and has to be used single-threadly with the Connection
class Statement {
    friend class Connection;

    MYSQL* conn;
    size_t* connection_referencing_objects_num;
    MYSQL_STMT* stmt;
    size_t field_count = 0;
    int uncaught_exceptions = std::uncaught_exceptions();
    bool store_result = true;

    explicit Statement(
        MYSQL* conn, size_t* connection_referencing_objects_num, MYSQL_STMT* stmt
    ) noexcept;

    template <class... Params>
    // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
    void do_bind_and_execute(Params&&... params);

    template <class... Params>
    void bind_and_execute(Params&&... params);

    struct Res {
        struct Bind {
            struct Numeric {};

            struct Bool {
                unsigned char bind_value;
                bool* dest;
            };

            struct String {
                ::InplaceBuff<0> buff;
                std::string* str;
            };

            struct InplaceBuff {
                InplaceBuffBase* inplace_buff;
            };

            template <class T>
            struct EnumWithStringConversions {
                T bind_value;
                void* dest;
                using Callback = void (*)(size_t column, T value, void* dest);
                Callback callback;
            };

            template <class T>
            struct OptionalNumeric {
                T value;
                std::optional<T>* dest;
            };

            struct OptionalBool {
                unsigned char value;
                std::optional<bool>* dest;
            };

            struct OptionalStringlike {
                ::InplaceBuff<0> buff;
                void* dest;
                using Callback = void (*)(const ::InplaceBuff<0>* buff, void* dest);
                Callback callback;
            };

            template <class T>
            struct OptionalEnumWithStringConversions {
                T bind_value;
                void* dest;
                using Callback = void (*)(size_t column, std::optional<T> value, void* dest);
                Callback callback;
            };

            std::variant<
                Bool,
                Numeric,
                String,
                InplaceBuff,
                EnumWithStringConversions<int8_t>,
                EnumWithStringConversions<int16_t>,
                EnumWithStringConversions<int32_t>,
                EnumWithStringConversions<int64_t>,
                EnumWithStringConversions<uint8_t>,
                EnumWithStringConversions<uint16_t>,
                EnumWithStringConversions<uint32_t>,
                EnumWithStringConversions<uint64_t>,
                OptionalNumeric<char>,
                OptionalNumeric<signed char>,
                OptionalNumeric<unsigned char>,
                OptionalNumeric<short>, // NOLINT(google-runtime-int)
                OptionalNumeric<unsigned short>, // NOLINT(google-runtime-int)
                OptionalNumeric<int>,
                OptionalNumeric<unsigned int>,
                OptionalNumeric<long long>, // NOLINT(google-runtime-int)
                OptionalNumeric<unsigned long long>, // NOLINT(google-runtime-int)
                OptionalNumeric<long>, // NOLINT(google-runtime-int)
                OptionalNumeric<unsigned long>, // NOLINT(google-runtime-int)
                OptionalBool,
                OptionalStringlike,
                OptionalEnumWithStringConversions<int8_t>,
                OptionalEnumWithStringConversions<int16_t>,
                OptionalEnumWithStringConversions<int32_t>,
                OptionalEnumWithStringConversions<int64_t>,
                OptionalEnumWithStringConversions<uint8_t>,
                OptionalEnumWithStringConversions<uint16_t>,
                OptionalEnumWithStringConversions<uint32_t>,
                OptionalEnumWithStringConversions<uint64_t>>
                kind;

            template <class T>
            struct IsEnumWithStringConversions : public std::false_type {};

            template <class T>
            struct IsEnumWithStringConversions<EnumWithStringConversions<T>>
            : public std::true_type {};

            template <class T>
            struct IsOptionalNumeric : public std::false_type {};

            template <class T>
            struct IsOptionalNumeric<OptionalNumeric<T>> : public std::true_type {};

            template <class T>
            struct IsOptionalEnumWithStringConversions : public std::false_type {};

            template <class T>
            struct IsOptionalEnumWithStringConversions<OptionalEnumWithStringConversions<T>>
            : public std::true_type {};
        };

        std::unique_ptr<Bind[]> binds;
        std::unique_ptr<MYSQL_BIND[]> mysql_binds;
        bool previous_fetch_changed_binds = false;
    } res;

public:
    ~Statement() noexcept(false);

    Statement(Statement&& other) noexcept;

    // There is a problem with libmariadb preventing implementation of this operator because of
    // getting "Commands out of sync; you can't run this command now" error. Consider following
    // code illustrating the problem:
    //
    // auto* stmt1 = mysql_stmt_init(conn);
    // throw_assert(stmt1);
    // throw_assert(!mysql_stmt_prepare(
    //     stmt1,
    //     "select 1 from users where id=1",
    //     strlen("select 1 from users where id=1")
    // ));
    // throw_assert(!mysql_stmt_execute(stmt1));
    //
    // int x1;
    // MYSQL_BIND binds1[1];
    // binds1[0].buffer_type = MYSQL_TYPE_LONG;
    // binds1[0].buffer = &x1;
    // binds1[0].is_unsigned = false;
    // binds1[0].error = &binds1[0].error_value;
    // binds1[0].is_null = &binds1[0].is_null_value;
    // throw_assert(!mysql_stmt_bind_result(stmt1, binds1));
    // throw_assert(!mysql_stmt_store_result(stmt1));
    //
    // auto stmt2 = mysql_stmt_init(conn);
    // throw_assert(stmt2);
    // if (mysql_stmt_prepare(
    //     stmt2,
    //     "select 1 from users where id=1",
    //     strlen("select 1 from users where id=1")
    // )) {
    //     THROW(mysql_stmt_error(stmt2));
    // }
    // throw_assert(!mysql_stmt_execute(stmt2));
    // // This is the root cause of the problem, this call has to be before
    // // mysql_stmt_execute(stmt2) for the problem to disappear.
    // throw_assert(!mysql_stmt_close(stmt1));
    //
    // int x2;
    // MYSQL_BIND binds2[1];
    // binds2[0].buffer_type = MYSQL_TYPE_LONG;
    // binds2[0].buffer = &x2;
    // binds2[0].is_unsigned = false;
    // binds2[0].error = &binds2[0].error_value;
    // binds2[0].is_null = &binds2[0].is_null_value;
    // throw_assert(!mysql_stmt_bind_result(stmt2, binds2));
    //
    // if (mysql_stmt_store_result(stmt2)) {
    //     THROW(mysql_stmt_error(stmt2)); // Here exception "Commands out of sync;" is thrown
    // }
    // throw_assert(!mysql_stmt_close(stmt2));
    Statement& operator=(Statement&& other) = delete;

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    // If called, must be called before res_bind(). Prevents client retrieving the whole result from
    // the server. This disallows executing other queries in parallel. It may be advantageous for
    // very big queries or even required if the result does not fit in memory.
    void do_not_store_result() noexcept;

    template <class... Refs>
    void res_bind(Refs&... refs);

    bool next();

    [[nodiscard]] uint64_t affected_rows() noexcept;

    [[nodiscard]] uint64_t insert_id() noexcept;
};

void run_unit_tests(sim::mysql::Connection& mysql);

/***************************************** IMPLEMENTATION *****************************************/

namespace detail {

template <class>
constexpr inline bool is_optional = false;
template <class T>
constexpr inline bool is_optional<std::optional<T>> = true;

template <class T>
constexpr inline auto buffer_type_of = [] {
    static_assert(std::is_integral_v<T>, "buffer_type_of works only with integral types");
    if constexpr (meta::is_one_of<T, char, signed char, unsigned char>) {
        return MYSQL_TYPE_TINY;
    } else if constexpr (meta::is_one_of<T, short, unsigned short>) { // NOLINT(google-runtime-int)
        return MYSQL_TYPE_SHORT;
    } else if constexpr (meta::is_one_of<T, int, unsigned int>) {
        return MYSQL_TYPE_LONG;
        // NOLINTNEXTLINE(google-runtime-int)
    } else if constexpr (meta::is_one_of<T, long long, unsigned long long>) {
        return MYSQL_TYPE_LONGLONG;
    } else if constexpr (meta::is_one_of<T, long, unsigned long>) { // NOLINT(google-runtime-int)
        if constexpr (sizeof(T) == sizeof(int)) {
            return MYSQL_TYPE_LONG;
        } else if constexpr (sizeof(T) == sizeof(long long)) { // NOLINT(google-runtime-int)
            return MYSQL_TYPE_LONGLONG;
        } else {
            static_assert(always_false<T>, "These is no equivalent for type long");
        }
    } else {
        static_assert(always_false<T>, "Unsupported type");
    }
}();

} // namespace detail

template <class SqlExpr, class>
Statement Connection::execute(SqlExpr&& sql_expr) {
    STACK_UNWINDING_MARK;

    auto sql = sql::SqlWithParams{std::forward<SqlExpr>(sql_expr)}; // may throw
    auto sql_str = std::move(sql).get_sql();
    bool retrying = false;
    for (;;) {
        MYSQL_STMT* stmt = mysql_stmt_init(conn);
        if (!stmt) {
            THROW(mysql_error(conn));
        }
        if (mysql_stmt_prepare(stmt, sql_str.data(), sql_str.length())) {
            bool safe_to_reconnect = referencing_objects_num == 0;
            if (!retrying && mysql_errno(conn) == CR_SERVER_GONE_ERROR && safe_to_reconnect) {
                try {
                    // mysql_real_connect() may be called only once on a connection -- a new one
                    // must be created.
                    auto new_connection =
                        Connection(conn->host, conn->user, conn->passwd, conn->db);
                    std::swap(conn, new_connection.conn); // won't throw
                    retrying = true;
                    // Needs to be done before new_connection is destructed because it contains
                    // connection the stmt is associated with.
                    (void)mysql_stmt_close(stmt);
                    continue;
                } catch (...) {
                    (void)mysql_stmt_close(stmt);
                    throw;
                }
            }
            try {
                THROW(mysql_error(conn), " ", mysql_errno(conn), " ", referencing_objects_num);
            } catch (...) {
                // This has to happen after mysql_error() otherwise the error string is empty
                (void)mysql_stmt_close(stmt);
                throw;
            }
        }

        auto res = Statement{conn, &referencing_objects_num, stmt};
        std::apply(
            [&res](auto&&... params) {
                res.bind_and_execute(std::forward<decltype(params)>(params)...);
            },
            std::move(sql).get_params()
        );
        return res;
    }
}

template <class... Params>
// NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
void Statement::do_bind_and_execute(Params&&... params) {
    assert(sizeof...(params) == mysql_stmt_param_count(stmt));
    field_count = mysql_stmt_field_count(stmt);

    std::array<MYSQL_BIND, sizeof...(params)> binds;
    if constexpr (binds.size() > 0) {
        std::memset(binds.data(), 0, sizeof(MYSQL_BIND) * binds.size());
    }
    static constexpr auto fill_bind_non_optional = [](MYSQL_BIND& bind, auto& param) {
        using Param = std::remove_const_t<std::remove_reference_t<decltype(param)>>;
        if constexpr (std::is_same_v<Param, std::nullptr_t>) {
            bind.buffer_type = MYSQL_TYPE_NULL;
        } else if constexpr (std::is_integral_v<Param>) {
            bind.buffer_type = detail::buffer_type_of<Param>;
            bind.buffer = &param;
            bind.is_unsigned = std::is_unsigned_v<Param>;
        } else if constexpr (std::is_same_v<Param, std::string_view>) {
            bind.buffer_type = MYSQL_TYPE_BLOB;
            // If buffer is nullptr, the mysql value is NULL, but we want an empty string here.
            bind.buffer = const_cast<char*>(param.empty() ? "" : param.data());
            bind.buffer_length = param.size();
            bind.length = nullptr;
            bind.length_value = param.size();
        } else {
            static_assert(always_false<Param>, "Unsupported type to pass as parameter");
        }
    };
    [[maybe_unused]] static constexpr auto fill_bind = [](MYSQL_BIND& bind, auto& param) {
        using Param = std::remove_const_t<std::remove_reference_t<decltype(param)>>;
        if constexpr (detail::is_optional<Param>) {
            if (param) {
                fill_bind_non_optional(bind, *param);
            } else {
                bind.buffer_type = MYSQL_TYPE_NULL;
            }
        } else {
            fill_bind_non_optional(bind, param);
        }
    };
    {
        size_t idx = 0;
        // Pass values as references so that bound references live up to mysql_stmt_execute().
        (fill_bind(binds[idx++], params), ...);
    }
    if (mysql_stmt_bind_param(stmt, binds.data())) {
        THROW(mysql_stmt_error(stmt));
    }
    if (mysql_stmt_execute(stmt)) {
        THROW(mysql_stmt_error(stmt));
    }
}

template <class... Params>
void Statement::bind_and_execute(Params&&... params) {
    static constexpr auto convert_non_optional_non_enum = [](auto&& param) -> decltype(auto) {
        using Param = std::remove_const_t<std::remove_reference_t<decltype(param)>>;
        if constexpr (std::is_same_v<Param, std::nullopt_t>) {
            return nullptr;
        } else if constexpr (std::is_same_v<Param, bool>) {
            return static_cast<unsigned char>(param ? 1 : 0);
        } else if constexpr (std::is_convertible_v<const Param&, StringView>) {
            auto sv = StringView{static_cast<const Param&>(param)};
            return std::string_view{sv.data(), sv.size()};
        } else if constexpr (std::is_same_v<decltype(param), const Param&> &&
                             std::is_trivially_copyable_v<Param>)
        {
            return Param{param};
        } else {
            return std::forward<decltype(param)>(param);
        }
    };
    static constexpr auto convert_non_optional = [](auto&& param) -> decltype(auto) {
        using Param = std::remove_const_t<std::remove_reference_t<decltype(param)>>;
        if constexpr (is_enum_with_string_conversions<Param>) {
            return static_cast<typename Param::UnderlyingType>(param);
        } else if constexpr (std::is_enum_v<Param>) {
            return static_cast<std::underlying_type_t<Param>>(param);
        } else {
            return convert_non_optional_non_enum(std::forward<decltype(param)>(param));
        }
    };
    [[maybe_unused]] static constexpr auto convert_param = [](auto&& param) -> decltype(auto) {
        using Param = std::remove_const_t<std::remove_reference_t<decltype(param)>>;
        if constexpr (detail::is_optional<Param>) {
            return param
                ? std::optional{convert_non_optional(*std::forward<decltype(param)>(param))}
                : std::nullopt;
        } else {
            return convert_non_optional(std::forward<decltype(param)>(param));
        }
    };
    do_bind_and_execute(convert_param(std::forward<Params>(params))...);
}

template <class... Refs>
void Statement::res_bind(Refs&... refs) {
    STACK_UNWINDING_MARK;

    if (sizeof...(refs) != field_count) {
        THROW("Invalid number of binds: ", sizeof...(refs), " expected ", field_count);
    }

    res.binds = std::make_unique<Res::Bind[]>(sizeof...(refs));
    res.mysql_binds = std::make_unique<MYSQL_BIND[]>(sizeof...(refs));
    std::memset(res.mysql_binds.get(), 0, sizeof(MYSQL_BIND) * sizeof...(refs));

    static constexpr auto fill_bind = [](Res::Bind& bind, MYSQL_BIND& mysql_bind, auto& ref) {
        // MYSQL_BIND::error_value is not filled if MYSQL_BIND::error == nullptr
        mysql_bind.error = &mysql_bind.error_value;
        // MYSQL_BIND::is_null_value is not filled if MYSQL_BIND::is_null == nullptr
        mysql_bind.is_null = &mysql_bind.is_null_value;

        using Ref = std::remove_reference_t<decltype(ref)>;
        if constexpr (std::is_same_v<Ref, bool>) {
            bind.kind = Res::Bind::Bool{
                .bind_value = 0,
                .dest = &ref,
            };
            mysql_bind.buffer_type = detail::buffer_type_of<decltype(Res::Bind::Bool::bind_value)>;
            mysql_bind.buffer = &std::get<Res::Bind::Bool>(bind.kind).bind_value;
            mysql_bind.is_unsigned = std::is_unsigned_v<decltype(Res::Bind::Bool::bind_value)>;
        } else if constexpr (std::is_integral_v<Ref>) {
            bind.kind = Res::Bind::Numeric{};
            mysql_bind.buffer_type = detail::buffer_type_of<Ref>;
            mysql_bind.buffer = &ref;
            mysql_bind.is_unsigned = std::is_unsigned_v<Ref>;
            // Accept types inheriting from std::string
        } else if constexpr (std::is_convertible_v<Ref&, std::string&>) {
            bind.kind = Res::Bind::String{
                .buff = {},
                .str = &ref,
            };
            auto& string = std::get<Res::Bind::String>(bind.kind);
            mysql_bind.buffer_type = MYSQL_TYPE_BLOB;
            mysql_bind.buffer = string.buff.data();
            mysql_bind.buffer_length = string.buff.max_size();
            mysql_bind.length = &string.buff.size;
        } else if constexpr (std::is_base_of_v<InplaceBuffBase, Ref>) {
            bind.kind = Res::Bind::InplaceBuff{
                .inplace_buff = &ref,
            };
            mysql_bind.buffer_type = MYSQL_TYPE_BLOB;
            mysql_bind.buffer = ref.data();
            mysql_bind.buffer_length = ref.max_size();
            mysql_bind.length = &ref.size;
        } else if constexpr (is_enum_with_string_conversions<Ref>) {
            using BindKind = Res::Bind::EnumWithStringConversions<typename Ref::UnderlyingType>;
            bind.kind = BindKind{
                .bind_value = 0,
                .dest = &ref,
                .callback =
                    [](size_t column, typename Ref::UnderlyingType value, void* dest) {
                        auto& ref = *static_cast<Ref*>(dest);
                        ref = Ref{value};
                        if (!ref.has_known_value()) {
                            THROW(
                                "Invalid value for ENUM_WITH_STRING_CONVERSIONS at column: ", column
                            );
                        }
                    },
            };
            mysql_bind.buffer_type = detail::buffer_type_of<decltype(BindKind::bind_value)>;
            mysql_bind.buffer = &std::get<BindKind>(bind.kind).bind_value;
            mysql_bind.is_unsigned = std::is_unsigned_v<decltype(BindKind::bind_value)>;
        } else if constexpr (detail::is_optional<Ref>) {
            using T = typename Ref::value_type;
            if constexpr (std::is_same_v<T, bool>) {
                bind.kind = Res::Bind::OptionalBool{
                    .value = 0,
                    .dest = &ref,
                };
                mysql_bind.buffer_type =
                    detail::buffer_type_of<decltype(Res::Bind::OptionalBool::value)>;
                mysql_bind.buffer = &std::get<Res::Bind::OptionalBool>(bind.kind).value;
                mysql_bind.is_unsigned =
                    std::is_unsigned_v<decltype(Res::Bind::OptionalBool::value)>;
            } else if constexpr (std::is_integral_v<T>) {
                bind.kind = Res::Bind::OptionalNumeric<T>{
                    .value = 0,
                    .dest = &ref,
                };
                mysql_bind.buffer_type = detail::buffer_type_of<T>;
                mysql_bind.buffer = &std::get<Res::Bind::OptionalNumeric<T>>(bind.kind).value;
                mysql_bind.is_unsigned = std::is_unsigned_v<T>;
            } else if constexpr (std::is_convertible_v<T&, std::string&>) {
                bind.kind = Res::Bind::OptionalStringlike{
                    .buff = {},
                    .dest = &ref,
                    .callback =
                        [](const ::InplaceBuff<0>* buff, void* dest) {
                            auto& ref = *static_cast<std::optional<T>*>(dest);
                            if (buff) {
                                if (ref) {
                                    static_cast<std::string&>(*ref).assign(
                                        buff->data(), buff->size
                                    );
                                } else {
                                    ref = std::string{buff->data(), buff->size};
                                }
                            } else {
                                ref = std::nullopt;
                            }
                        },
                };
                auto& opt_from_str = std::get<Res::Bind::OptionalStringlike>(bind.kind);
                mysql_bind.buffer_type = MYSQL_TYPE_BLOB;
                mysql_bind.buffer = opt_from_str.buff.data();
                mysql_bind.buffer_length = opt_from_str.buff.max_size();
                mysql_bind.length = &opt_from_str.buff.size;
            } else if constexpr (is_enum_with_string_conversions<T>) {
                using BindKind =
                    Res::Bind::OptionalEnumWithStringConversions<typename T::UnderlyingType>;
                bind.kind = BindKind{
                    .bind_value = 0,
                    .dest = &ref,
                    .callback =
                        [](size_t column,
                           std::optional<typename T::UnderlyingType> value,
                           void* dest) {
                            auto& ref = *static_cast<std::optional<T>*>(dest);
                            if (value) {
                                ref = T{*value};
                                if (!ref->has_known_value()) {
                                    THROW(
                                        "Invalid value for ENUM_WITH_STRING_CONVERSIONS at "
                                        "column: ",
                                        column
                                    );
                                }
                            } else {
                                ref = std::nullopt;
                            }
                        },
                };
                mysql_bind.buffer_type = detail::buffer_type_of<decltype(BindKind::bind_value)>;
                mysql_bind.buffer = &std::get<BindKind>(bind.kind).bind_value;
                mysql_bind.is_unsigned = std::is_unsigned_v<decltype(BindKind::bind_value)>;
            } else {
                static_assert(always_false<Ref>, "Unsupported type to bind");
            }
        } else {
            static_assert(always_false<Ref>, "Unsupported type to bind");
        }
    };
    {
        size_t idx = 0;
        ((fill_bind(res.binds[idx], res.mysql_binds[idx], refs), ++idx), ...);
    }
    if (mysql_stmt_bind_result(stmt, res.mysql_binds.get())) {
        THROW(mysql_stmt_error(stmt));
    }
    // Allows executing more than one query in parallel and speeds up retrieving results.
    if (store_result && mysql_stmt_store_result(stmt)) {
        THROW(mysql_stmt_error(stmt));
    }
    res.previous_fetch_changed_binds = false;
}

} // namespace sim::mysql

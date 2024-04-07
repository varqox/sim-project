#pragma once

#include <cassert>
#include <cstddef>
#include <cstring>
#include <exception>
#include <memory>
#include <mysql.h>
#include <optional>
#include <sim/sqlv2/sqlv2.hh>
#include <simlib/always_false.hh>
#include <simlib/inplace_buff.hh>
#include <simlib/macros/enum_with_string_conversions.hh>
#include <simlib/macros/throw.hh>
#include <simlib/meta/is_one_of.hh>
#include <simlib/string_view.hh>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace sim::mysqlv2 {

class Statement;

class ConnectionView {
    MYSQL* conn;

public:
    explicit ConnectionView(MYSQL* connection) noexcept : conn{connection} {}

    ~ConnectionView() = default;

    ConnectionView(const ConnectionView&) = delete;
    ConnectionView(ConnectionView&&) = delete;
    ConnectionView& operator=(const ConnectionView&) = delete;
    ConnectionView& operator=(ConnectionView&&) = delete;

    void update(std::string_view sql);

    template <class SqlExpr, class = decltype(sqlv2::SqlWithParams{std::declval<SqlExpr&&>()})>
    Statement execute(SqlExpr&& sql_expr);
};

class Statement {
    friend class ConnectionView;

    MYSQL* conn;
    MYSQL_STMT* stmt;
    size_t field_count = 0;
    int uncaught_exceptions = std::uncaught_exceptions();
    bool store_result = true;

    explicit Statement(MYSQL* conn, MYSQL_STMT* stmt) noexcept : conn{conn}, stmt{stmt} {}

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

            struct OptionalString {
                ::InplaceBuff<0> buff;
                std::optional<std::string>* dest;
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
                OptionalString,
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
        bool previous_fetch_changed_binds;
    } res;

public:
    ~Statement() noexcept(false);

    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&& other) noexcept;

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    // Prevents client retrieving the whole result from the server. This disallows executing other
    // queries in parallel. If called, must be called before res_bind(). It may be advantageous for
    // very big queries or even required if the result does not fit in memory.
    void do_not_store_result() noexcept;

    template <class... Refs>
    void res_bind(Refs&... refs);

    bool next();

    [[nodiscard]] uint64_t affected_rows() noexcept;

    [[nodiscard]] uint64_t insert_id() noexcept;
};

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

inline void ConnectionView::update(std::string_view sql) {
    if (mysql_real_query(conn, sql.data(), sql.size())) {
        THROW(mysql_error(conn));
    }
}

template <class SqlExpr, class>
Statement ConnectionView::execute(SqlExpr&& sql_expr) {
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) {
        THROW(mysql_error(conn));
    }
    auto res = Statement{conn, stmt};
    auto sql = sqlv2::SqlWithParams{std::forward<SqlExpr>(sql_expr)};
    auto sql_str = std::move(sql).get_sql();
    if (mysql_stmt_prepare(stmt, sql_str.data(), sql_str.length())) {
        THROW(mysql_error(conn));
    }

    std::apply(
        [&res](auto&&... params) {
            res.bind_and_execute(std::forward<decltype(params)>(params)...);
        },
        std::move(sql).get_params()
    );
    return res;
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
        } else if constexpr (std::is_same_v<decltype(param), const Param&> && std::is_trivially_copyable_v<Param>)
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

inline Statement::~Statement() noexcept(false) {
    if (stmt && mysql_stmt_close(stmt) && uncaught_exceptions == std::uncaught_exceptions()) {
        THROW(mysql_error(conn));
    }
}

inline Statement::Statement(Statement&& other) noexcept
: conn{other.conn}
, stmt{std::exchange(other.stmt, nullptr)}
, field_count{other.field_count}
, res{std::move(other.res)} {}

inline Statement& Statement::operator=(Statement&& other) noexcept {
    if (this != &other) {
        conn = other.conn;
        stmt = std::exchange(other.stmt, nullptr);
        field_count = other.field_count;
        res = std::move(other.res);
    }
    return *this;
}

inline void Statement::do_not_store_result() noexcept { store_result = false; }

template <class... Refs>
void Statement::res_bind(Refs&... refs) {
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
            } else if constexpr (std::is_same_v<T, std::string>) {
                bind.kind = Res::Bind::OptionalString{
                    .buff = {},
                    .dest = &ref,
                };
                auto& opt_string = std::get<Res::Bind::OptionalString>(bind.kind);
                mysql_bind.buffer_type = MYSQL_TYPE_BLOB;
                mysql_bind.buffer = opt_string.buff.data();
                mysql_bind.buffer_length = opt_string.buff.max_size();
                mysql_bind.length = &opt_string.buff.size;
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

inline bool Statement::next() {
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
                } else if constexpr (std::is_same_v<Kind, Res::Bind::OptionalString>) {
                    if (mysql_bind.is_null_value) {
                        *bind.dest = std::nullopt;
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
                        if (*bind.dest) {
                            (*bind.dest)->assign(inplace_buff.data(), inplace_buff.size);
                        } else {
                            *bind.dest = std::string{inplace_buff.data(), inplace_buff.size};
                        }
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

inline uint64_t Statement::affected_rows() noexcept { return mysql_stmt_affected_rows(stmt); }

inline uint64_t Statement::insert_id() noexcept { return mysql_stmt_insert_id(stmt); }

} // namespace sim::mysqlv2

/********************************************* TESTS *********************************************/

#if 0
#include <cstdint>
#include <simlib/defer.hh>
#include <simlib/enum_to_underlying_type.hh>
#include <simlib/macros/stack_unwinding.hh>
#include <simlib/string_traits.hh>
#include <simlib/throw_assert.hh>

#define assert_throws(code, description)                   \
    try {                                                  \
        code;                                              \
    } catch (const std::exception& e) {                    \
        throw_assert(has_prefix(e.what(), (description))); \
    }

namespace sim::mysqlv2 {

ENUM_WITH_STRING_CONVERSIONS(EWSC, uint8_t,
    (A, 0, "aa")
    (B, 7, "bb")
    (C, 42, "cc")
);

inline void test(sim::mysqlv2::ConnectionView& cv) {
    STACK_UNWINDING_MARK;

    using sim::sqlv2::InsertInto;
    using sim::sqlv2::Select;
    using sim::sqlv2::SqlWithParams;
    // nullptr and std::nullopt
    {
        cv.execute(sim::sqlv2::SqlWithParams(
            "CREATE TABLE test(id bigint unsigned NOT NULL, x int DEFAULT NULL, PRIMARY KEY(id))"
        ));
        auto dropper = Defer{[&] { cv.execute(SqlWithParams("DROP TABLE test")); }};
        cv.execute(InsertInto("test(id, x)").values("1, ?", nullptr));
        cv.execute(InsertInto("test(id, x)").values("2, ?", std::nullopt));

        // optional<int>
        auto stmt = cv.execute(Select("x").from("test").order_by("id"));
        std::optional<int> oi;
        stmt.res_bind(oi);
        throw_assert(stmt.next() && oi == std::nullopt);
        throw_assert(stmt.next() && oi == std::nullopt);
        throw_assert(!stmt.next());
    }
    // bool and optional<bool>
    {
        cv.execute(
            SqlWithParams("CREATE TABLE test(id bigint unsigned NOT NULL, x tinyint DEFAULT NULL, "
                          "PRIMARY KEY(id))")
        );
        auto dropper = Defer{[&] { cv.execute(SqlWithParams("DROP TABLE test")); }};
        cv.execute(InsertInto("test(id, x)").values("1, ?", true));
        cv.execute(InsertInto("test(id, x)").values("2, ?", nullptr));
        cv.execute(InsertInto("test(id, x)").values("3, ?", std::optional{false}));

        // bool
        auto stmt = cv.execute(Select("x").from("test").order_by("id"));
        bool b;
        stmt.res_bind(b);
        throw_assert(stmt.next() && b == true);
        assert_throws(
            stmt.next(), "Unexpected NULL at column: 0 (bool& does not accept NULL values)"
        );
        throw_assert(stmt.next() && b == false);
        throw_assert(!stmt.next());

        // optional<bool>
        stmt = cv.execute(Select("x").from("test").order_by("id"));
        std::optional<bool> ob;
        stmt.res_bind(ob);
        throw_assert(stmt.next() && ob == true);
        throw_assert(stmt.next() && ob == std::nullopt);
        throw_assert(stmt.next() && ob == false);
        throw_assert(!stmt.next());

        // Truncation
        stmt = cv.execute(SqlWithParams("SELECT 2"));
        stmt.res_bind(b);
        assert_throws(stmt.next(), "Truncated data at column: 0");
    }
    // signed short and optional<signed short>
    {
        cv.execute(
            SqlWithParams("CREATE TABLE test(id bigint unsigned NOT NULL, x smallint DEFAULT NULL, "
                          "PRIMARY KEY(id))")
        );
        auto dropper = Defer{[&] { cv.execute(SqlWithParams("DROP TABLE test")); }};
        cv.execute(InsertInto("test(id, x)").values("1, ?", -1));
        cv.execute(InsertInto("test(id, x)").values("2, ?", nullptr));
        cv.execute(InsertInto("test(id, x)").values("3, ?", std::optional{-2}));

        // signed short
        auto stmt = cv.execute(Select("x").from("test").order_by("id"));
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
        stmt = cv.execute(Select("x").from("test").order_by("id"));
        std::optional<int16_t> oi;
        stmt.res_bind(oi);
        throw_assert(stmt.next() && oi == -1);
        throw_assert(stmt.next() && oi == std::nullopt);
        throw_assert(stmt.next() && oi == -2);
        throw_assert(!stmt.next());

        // Truncation
        stmt = cv.execute(SqlWithParams("SELECT 1000000"));
        stmt.res_bind(i);
        assert_throws(stmt.next(), "Truncated data at column: 0");
    }
    // unsigned short and optional<unsigned short>
    {
        cv.execute(SqlWithParams(
            "CREATE TABLE test(id bigint unsigned NOT NULL, x smallint unsigned DEFAULT "
            "NULL, PRIMARY KEY(id))"
        ));
        auto dropper = Defer{[&] { cv.execute(SqlWithParams("DROP TABLE test")); }};
        cv.execute(InsertInto("test(id, x)").values("1, ?", 7));
        cv.execute(InsertInto("test(id, x)").values("2, ?", nullptr));
        cv.execute(InsertInto("test(id, x)").values("3, ?", std::optional{42}));

        // unsigned short
        auto stmt = cv.execute(Select("x").from("test").order_by("id"));
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
        stmt = cv.execute(Select("x").from("test").order_by("id"));
        std::optional<uint16_t> oi;
        stmt.res_bind(oi);
        throw_assert(stmt.next() && oi == 7);
        throw_assert(stmt.next() && oi == std::nullopt);
        throw_assert(stmt.next() && oi == 42);
        throw_assert(!stmt.next());

        // Truncation
        stmt = cv.execute(SqlWithParams("SELECT -1"));
        stmt.res_bind(i);
        assert_throws(stmt.next(), "Truncated data at column: 0");
    }
    // std::string and optional<std::string>
    {
        cv.execute(SqlWithParams(
            "CREATE TABLE test(id bigint unsigned NOT NULL, x VARBINARY(10) DEFAULT NULL, "
            "PRIMARY KEY(id))"
        ));
        auto dropper = Defer{[&] { cv.execute(SqlWithParams("DROP TABLE test")); }};
        cv.execute(InsertInto("test(id, x)").values("1, ?", "aaa"));
        cv.execute(InsertInto("test(id, x)").values("2, ?", nullptr));
        cv.execute(InsertInto("test(id, x)").values("3, ?", std::string{"bbb"}));
        cv.execute(InsertInto("test(id, x)").values("4, ?", std::string_view{"ccc ddd"}));
        cv.execute(InsertInto("test(id, x)").values("5, ?", StringView{"eee"}));
        cv.execute(InsertInto("test(id, x)").values("6, ?", std::optional{InplaceBuff<3>{""}}));
        cv.execute(InsertInto("test(id, x)").values("7, ?", std::optional{std::string{"xxx"}}));

        // std::string
        auto stmt = cv.execute(Select("x").from("test").order_by("id"));
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
        stmt = cv.execute(Select("x").from("test").order_by("id"));
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
        cv.execute(SqlWithParams(
            "CREATE TABLE test(id bigint unsigned NOT NULL, x VARBINARY(10) DEFAULT NULL, "
            "PRIMARY KEY(id))"
        ));
        auto dropper = Defer{[&] { cv.execute(SqlWithParams("DROP TABLE test")); }};
        cv.execute(InsertInto("test(id, x)").values("1, ?", "aaa"));
        cv.execute(InsertInto("test(id, x)").values("2, ?", nullptr));
        cv.execute(InsertInto("test(id, x)").values("3, ?", std::string{"bbb"}));
        cv.execute(InsertInto("test(id, x)").values("4, ?", std::string_view{"ccc ddd"}));
        cv.execute(InsertInto("test(id, x)").values("5, ?", StringView{"eee"}));
        cv.execute(InsertInto("test(id, x)").values("6, ?", std::optional{InplaceBuff<3>{""}}));
        cv.execute(InsertInto("test(id, x)").values("7, ?", std::optional{std::string{"xxx"}}));

        // InplaceBuff
        auto stmt = cv.execute(Select("x").from("test").order_by("id"));
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

        cv.execute(SqlWithParams(
            "CREATE TABLE test(id bigint unsigned NOT NULL, x int unsigned DEFAULT NULL, "
            "PRIMARY KEY(id))"
        ));
        auto dropper = Defer{[&] { cv.execute(SqlWithParams("DROP TABLE test")); }};
        cv.execute(InsertInto("test(id, x)").values("1, ?", Enum::A));
        cv.execute(InsertInto("test(id, x)").values("2, ?", Enum::B));
        cv.execute(InsertInto("test(id, x)").values("3, ?", std::optional<Enum>{std::nullopt}));
        cv.execute(InsertInto("test(id, x)").values("4, ?", std::optional{Enum::C}));

        std::optional<int> x;
        auto stmt = cv.execute(Select("x").from("test").order_by("id"));
        stmt.res_bind(x);
        throw_assert(stmt.next() && x == enum_to_underlying_type(Enum::A));
        throw_assert(stmt.next() && x == enum_to_underlying_type(Enum::B));
        throw_assert(stmt.next() && x == std::nullopt);
        throw_assert(stmt.next() && x == enum_to_underlying_type(Enum::C));
        throw_assert(!stmt.next());
    }
    // ENUM_WITH_STRING_CONVERSIONS
    {
        cv.execute(SqlWithParams(
            "CREATE TABLE test(id bigint unsigned NOT NULL, x tinyint unsigned DEFAULT NULL, "
            "PRIMARY KEY(id))"
        ));
        auto dropper = Defer{[&] { cv.execute(SqlWithParams("DROP TABLE test")); }};
        cv.execute(InsertInto("test(id, x)").values("1, ?", EWSC::A)); // enum
        cv.execute(InsertInto("test(id, x)").values("2, ?", EWSC{EWSC::B})); // EWSC
        cv.execute(InsertInto("test(id, x)").values("3, ?", std::optional<EWSC>{std::nullopt}));
        cv.execute(InsertInto("test(id, x)").values("4, ?", std::optional<EWSC>{EWSC::C}));

        // ENUM_WITH_STRING_CONVERSIONS
        auto stmt = cv.execute(Select("x").from("test").order_by("id"));
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
        stmt = cv.execute(SqlWithParams("SELECT 3"));
        stmt.res_bind(e);
        assert_throws(stmt.next(), "Invalid value for ENUM_WITH_STRING_CONVERSIONS at column: 0");

        // optional<ENUM_WITH_STRING_CONVERSIONS>
        stmt = cv.execute(Select("x").from("test").order_by("id"));
        std::optional<EWSC> oe;
        stmt.res_bind(oe);
        throw_assert(stmt.next() && oe == EWSC::A);
        throw_assert(stmt.next() && oe == EWSC::B);
        throw_assert(stmt.next() && oe == std::nullopt);
        throw_assert(stmt.next() && oe == EWSC::C);
        // Unmapped value
        stmt = cv.execute(SqlWithParams("SELECT 3"));
        stmt.res_bind(oe);
        assert_throws(stmt.next(), "Invalid value for ENUM_WITH_STRING_CONVERSIONS at column: 0");

        // Truncation
        stmt = cv.execute(SqlWithParams("SELECT -1"));
        stmt.res_bind(e);
        assert_throws(stmt.next(), "Truncated data at column: 0");
    }
}

} // namespace sim::mysqlv2

#endif

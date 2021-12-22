#pragma once

#include "simlib/concat_tostr.hh"
#include "simlib/file_path.hh"
#include "simlib/string_compare.hh"
#include "simlib/string_transform.hh"

#include <map>
#include <utility>
#include <vector>

class ConfigFile {
public:
    class ParseError : public std::runtime_error {
        std::string diagnostics_;

    public:
        explicit ParseError(const std::string& msg)
        : runtime_error(msg) {}

        template <class... Args, std::enable_if_t<(is_string_argument<Args> and ...), int> = 0>
        ParseError(size_t line, size_t pos, Args&&... msg)
        : runtime_error(
                  concat_tostr("line ", line, ':', pos, ": ", std::forward<Args>(msg)...)) {}

        ParseError(const ParseError& pe) = default;
        ParseError(ParseError&&) noexcept = default;
        ParseError& operator=(const ParseError& pe) = default;
        ParseError& operator=(ParseError&&) noexcept = default;

        using runtime_error::what;

        [[nodiscard]] const std::string& diagnostics() const noexcept { return diagnostics_; }

        ~ParseError() noexcept override = default;

        friend class ConfigFile;
    };

    class Variable {
    public:
        static constexpr uint8_t SET = 1; // set if variable appears in the
                                          // config
        static constexpr uint8_t ARRAY = 2; // set if variable is an array

        struct ValueSpan {
            size_t beg = 0;
            size_t end = 0;
        };

    private:
        uint8_t flag_ = 0;
        std::string str_;
        std::vector<std::string> arr_;
        ValueSpan value_span_; // value occupies [beg, end) range of the file

        void unset() noexcept {
            flag_ = 0;
            str_.clear();
            arr_.clear();
        }

    public:
        Variable() {} // NOLINT(modernize-use-equals-default): compiler bug

        [[nodiscard]] bool is_set() const noexcept { return flag_ & SET; }

        [[nodiscard]] bool is_array() const noexcept { return flag_ & ARRAY; }

        // Returns value as bool or false on error
        [[nodiscard]] bool as_bool() const noexcept {
            return (str_ == "1" || lower_equal(str_, "on") || lower_equal(str_, "true"));
        }

        template <class T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
        [[nodiscard]] std::optional<T> as() const noexcept {
            return str2num<T>(str_);
        }

        // Returns value as string (empty if not a string or variable isn't
        // set)
        [[nodiscard]] const std::string& as_string() const noexcept { return str_; }

        // Returns value as array (empty if not an array or variable isn't set)
        [[nodiscard]] const std::vector<std::string>& as_array() const noexcept {
            return arr_;
        }

        [[nodiscard]] ValueSpan value_span() const noexcept { return value_span_; }

        friend class ConfigFile;
    };

private:
    std::map<std::string, Variable, std::less<>> vars; // (name => value)
    // TODO: ^ maybe StringView would be better?
    static inline const Variable null_var{};

public:
    ConfigFile() = default;

    ConfigFile(const ConfigFile&) = default;
    ConfigFile(ConfigFile&&) noexcept = default;
    ConfigFile& operator=(const ConfigFile&) = default;
    ConfigFile& operator=(ConfigFile&&) noexcept = default;

    ~ConfigFile() = default;

    // Adds variables @p names to variable set, ignores duplications
    template <class... Args>
    void add_vars(Args&&... names) {
        int t[] = {(vars.emplace(std::forward<Args>(names), Variable{}), 0)...};
        (void)t;
    }

    // Clears variable set
    void clear() { vars.clear(); }

    // Returns a reference to a variable @p name from variable set or to a
    // null_var
    [[nodiscard]] const Variable& get_var(const StringView& name) const noexcept {
        return (*this)[name];
    }

    const Variable& operator[](const StringView& name) const noexcept {
        auto it = vars.find(name);
        return (it != vars.end() ? it->second : null_var);
    }

    [[nodiscard]] const decltype(vars)& get_vars() const { return vars; }

    /**
     * @brief Loads config (variables) form file @p pathname
     * @details Uses load_config_from_string()
     *
     * @param pathname config file
     * @param load_all whether load all variables from @p pathname or load only
     *   these from variable set
     *
     * @errors Throws an exception std::runtime_error if an open(2) error
     *   occurs and all exceptions from load_config_from_string()
     */
    void load_config_from_file(FilePath pathname, bool load_all = false);

    /**
     * @brief Loads config (variables) form string @p config
     *
     * @param config input string
     * @param load_all whether load all variables from @p config or load only
     *   these from variable set
     *
     * @errors Throws an exception (ParseError) if an error occurs
     */
    void load_config_from_string(std::string config, bool load_all = false);

    // Check if string @p str is a valid string literal
    static bool is_string_literal(StringView str) noexcept;

    /**
     * @brief Escapes unsafe sequences in str
     * @details '\'' replaces with "''"
     *
     * @param str input string
     *
     * @return escaped, single-quoted string
     */
    static std::string escape_to_single_quoted_string(StringView str);

    /**
     * @brief Escapes unsafe sequences in str
     * @details Escapes '\"' with "\\\"" and characters for which
     *   is_cntrl(3) != 0 with "\\xnn" sequence where n is a hexadecimal digit.
     *
     * @param str input string
     *
     * @return escaped, double-quoted string
     */
    static std::string escape_to_double_quoted_string(StringView str);

    /**
     * @brief Escapes unsafe sequences in str
     * @details Escapes '\"' with "\\\"" and characters for which
     *   is_print(3) == 0 with "\\xnn" sequence where n is a hexadecimal digit.
     *
     *   The difference between escape_to_double_quoted_string() and this
     *   function is that characters (bytes) not from interval [0, 127] are also
     *   escaped hexadecimally, that is why this option is not recommended when
     *   using utf-8 encoding - it will escape every non-ascii character (byte)
     *
     * @param str input string
     *
     * @return escaped, double-quoted string
     */
    static std::string full_escape_to_double_quoted_string(StringView str);

    /**
     * @brief Converts string @p str so that it can be safely placed in config
     *   file
     * @details Possible cases:
     *   1) @p str contains '\'' or character for which iscrtnl(3) != 0:
     *     Escaped @p str via escape_to_double_quoted_string() will be returned.
     *   2) Otherwise if @p str is string literal (is_string_literal(@p str)):
     *     Unchanged @p str will be returned.
     *   3) Otherwise:
     *     Escaped @p str via escape_to_single_quoted_string() will be returned.
     *
     *   Examples:
     *     "" -> '' (single quoted string, special case)
     *     "foo-bar" -> foo-bar (string literal)
     *     "line: 1\nab d E\n" -> "line: 1\nab d E\n" (double quoted string)
     *     " My awesome text" -> ' My awesome text' (single quoted string)
     *     " \\\\\\\\ " -> ' \\\\ ' (single quoted string)
     *     " '\t\n' ś " -> " '\t\n' ś " (double quoted string)
     *
     * @param str input string
     *
     * @return escaped (and possibly quoted) string
     */
    static std::string escape_string(StringView str);

    /**
     * @brief Converts string @p str so that it can be safely placed in config
     *   file
     * @details Possible cases:
     *   1) @p str contains '\'' or character for which is_print(3) == 0:
     *     Escaped @p str via full_escape_to_double_quoted_string() will be
     *     returned.
     *   2) Otherwise if @p str is string literal (is_string_literal(@p str)):
     *     Unchanged @p str will be returned.
     *   3) Otherwise:
     *     Escaped @p str via escape_to_single_quoted_string() will be returned.
     *
     *   The difference between escape_string() and this function is that
     *   characters not from interval [0, 127] are also escaped hexadecimally
     *   with full_escape_to_double_quoted_string()
     *
     *   Examples:
     *     "" -> '' (single quoted string, special case)
     *     "foo-bar" -> foo-bar (string literal)
     *     "line: 1\nab d E\n" -> "line: 1\nab d E\n" (double quoted string)
     *     " My awesome text" -> ' My awesome text' (single quoted string)
     *     " \\\\\\\\ " -> ' \\\\ ' (single quoted string)
     *     " '\t\n' ś " -> " '\t\n' \xc5\x9b " (double quoted string)
     *
     * @param str input string
     *
     * @return escaped (and possibly quoted) string
     */
    static std::string full_escape_string(StringView str);
};

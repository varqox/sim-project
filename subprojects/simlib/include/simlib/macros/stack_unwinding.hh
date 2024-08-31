#pragma once

#include <cstdint>
#include <exception>
#include <simlib/inplace_array.hh>
#include <simlib/inplace_buff.hh>
#include <simlib/logger.hh>
#include <simlib/macros/stringify.hh>

namespace stack_unwinding {

namespace detail {

constexpr bool is_va_empty() { return true; }

template <class T1, class... T>
constexpr bool is_va_empty(T1&& /*unused*/, T&&... /*unused*/) {
    return false;
}

constexpr const char* what_of() { return ""; }

inline const char* what_of(const std::exception& e) { return e.what(); }

} // namespace detail

class StackGuard {
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static inline thread_local uintmax_t event_stamp;

    bool inside_catch_ = static_cast<bool>(std::current_exception());
    int uncaught_counter_ = std::uncaught_exceptions();
    uintmax_t creation_stamp_ = event_stamp++;
    const char* file_;
    size_t line_;
    const char* pretty_func_;

public:
    struct StackMark {
        InplaceBuff<512> description;
        uintmax_t stamp = event_stamp++;

        template <class... Args, std::enable_if_t<(is_string_argument<Args> and ...), int> = 0>
        explicit StackMark(Args&&... args)
        : description(std::in_place, std::forward<Args>(args)...) {}
    };

    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static inline thread_local InplaceArray<StackMark, 128> marks_collected;

    StackGuard(const char* file, size_t line, const char* pretty_function) noexcept
    : file_(file)
    , line_(line)
    , pretty_func_(pretty_function) {}

    StackGuard(const StackGuard&) = delete;
    StackGuard(StackGuard&&) = delete;
    StackGuard& operator=(const StackGuard&) = delete;
    StackGuard& operator=(StackGuard&&) = delete;

    ~StackGuard() {
        if (std::uncaught_exceptions() == 1 and uncaught_counter_ == 0 and not inside_catch_) {
            // Remove stack marks that are from earlier exceptions
            if (marks_collected.size() > 0 and marks_collected.back().stamp < creation_stamp_) {
                marks_collected.clear();
            }

            try {
                StackMark a;
                StackMark b;
                assert(a.stamp + 1 == b.stamp);
                marks_collected.emplace_back(pretty_func_, " at ", file_, ':', line_);
            } catch (...) {
                // Nothing we can do here
            }
        }
    }
};

} // namespace stack_unwinding

#define STACK_UNWINDING_MARK_CONCATENATE_DETAIL(x, y) x##y
#define STACK_UNWINDING_MARK_CONCAT(x, y) STACK_UNWINDING_MARK_CONCATENATE_DETAIL(x, y)

#define STACK_UNWINDING_MARK                                         \
    const ::stack_unwinding::StackGuard                              \
    STACK_UNWINDING_MARK_CONCAT(stack_unwind_mark_no_, __COUNTER__)( \
        __FILE__, __LINE__, __PRETTY_FUNCTION__                      \
    )

#define ERRLOG_CATCH(...)                                                       \
    [&] {                                                                       \
        auto tmplog = errlog(                                                   \
            __FILE__ ":" STRINGIFY(__LINE__) ": Caught exception",              \
            ::stack_unwinding::detail::is_va_empty(__VA_ARGS__) ? "" : " -> ",  \
            ::stack_unwinding::detail::what_of(__VA_ARGS__),                    \
            "\nStack unwinding marks:\n"                                        \
        );                                                                      \
                                                                                \
        size_t i = 0; /* NOLINT(misc-const-correctness) */                      \
        for (auto const& mark : ::stack_unwinding::StackGuard::marks_collected) \
            tmplog('[', i++, "] ", mark.description, "\n");                     \
                                                                                \
        tmplog.flush_no_nl();                                                   \
        ::stack_unwinding::StackGuard::marks_collected.clear();                 \
    }()

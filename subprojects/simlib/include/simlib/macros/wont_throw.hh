#pragma once

#include <cstdio>
#include <exception>

class ThrowingIsBug {
    bool thrown = true;
    const char* file_str;
    size_t line;
    const char* expr_str;

public:
    template <class T>
    decltype(auto) evaluate(T&& expr) noexcept {
        thrown = false;
        return static_cast<T&&>(expr);
    }

    ThrowingIsBug(const char* file_str, size_t line, const char* expr_str)
    : file_str{file_str}
    , line{line}
    , expr_str{expr_str} {}

    ThrowingIsBug(const ThrowingIsBug&) = delete;
    ThrowingIsBug(ThrowingIsBug&&) = delete;
    ThrowingIsBug& operator=(const ThrowingIsBug&) = delete;
    ThrowingIsBug& operator=(ThrowingIsBug&&) = delete;

    ~ThrowingIsBug() {
        if (thrown) {
            (void)fprintf(
                stderr,
                "BUG: this was not expected to throw: %s (at %s:%zu)\n",
                expr_str,
                file_str,
                line
            );
            std::terminate();
        }
    }
};

// Sadly it won't preserve prvalue-ness, a prvalue will become an xvalue
#define WONT_THROW(...) (ThrowingIsBug(__FILE__, __LINE__, #__VA_ARGS__).evaluate(__VA_ARGS__))

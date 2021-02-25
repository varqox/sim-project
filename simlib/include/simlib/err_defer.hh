#pragma once

#include <exception>
#include <utility>

template <class Func>
class ErrDefer {
    int uncaught_exceptions = std::uncaught_exceptions();
    Func func_;

public:
    // NOLINTNEXTLINE(google-explicit-constructor)
    ErrDefer(Func func) try
    : func_(std::move(func))
    {
    } catch (...) {
        func();
        throw;
    }

    ErrDefer(const ErrDefer&) = delete;
    ErrDefer(ErrDefer&&) = delete;
    ErrDefer& operator=(const ErrDefer&) = delete;
    ErrDefer& operator=(ErrDefer&&) = delete;

    ~ErrDefer() {
        if (uncaught_exceptions != std::uncaught_exceptions()) {
            try {
                func_();
            } catch (...) {
            }
        }
    }
};

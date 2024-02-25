#pragma once

extern "C" void __attribute__((weak)) __ubsan_handle_builtin_unreachable(void*); // NOLINT

static inline const bool UNDEFINED_SANITIZER = &__ubsan_handle_builtin_unreachable;

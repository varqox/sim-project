#pragma once

extern "C" void __attribute__((weak)) __lsan_enable(void*); // NOLINT

static inline const bool LEAK_SANITIZER = &__lsan_enable;

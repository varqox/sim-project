#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void push(uint32_t seed);

uint32_t pull();

void reset();

void ncg(uint32_t seed);

#ifdef __cplusplus
}
#endif

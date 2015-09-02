#pragma once

#include <stdint.h>

void push(uint32_t seed);

uint32_t pull();

void reset();

void ncg(uint32_t seed);

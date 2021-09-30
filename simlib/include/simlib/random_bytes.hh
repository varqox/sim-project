#pragma once

#include "simlib/random.hh"

#include <string>

inline std::string random_bytes(size_t len) {
    std::string bytes(len, '\0');
    fill_randomly(bytes.data(), bytes.size());
    return bytes;
}

#pragma once

#include <simlib/errmsg.hh>
#include <simlib/macros/throw.hh>
#include <sys/mman.h>

// This will SIGKILL the process rather than failing allocation
inline void try_use_lots_of_memory(size_t how_much_in_bytes) {
    if (mmap(
            nullptr,
            how_much_in_bytes,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE,
            -1,
            0
        ) == MAP_FAILED) // NOLINT(performance-no-int-to-ptr)
    {
        THROW("mmap()", errmsg());
    }
}

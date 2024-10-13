#pragma once

enum class AmIRoot {
    YES,
    NO,
    MAYBE,
};

// Checks if effective UID or effective GID is equal to root UID / GID. Works in linux namespaces.
AmIRoot am_i_root();

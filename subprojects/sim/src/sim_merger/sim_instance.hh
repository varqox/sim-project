#pragma once

#include <sim/mysql/mysql.hh>
#include <string>

struct SimInstance {
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    const std::string& path; // with trailing '/'
    sim::mysql::Connection& mysql; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};

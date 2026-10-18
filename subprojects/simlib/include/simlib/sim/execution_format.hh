#pragma once

#include <cstdint>
#include <simlib/macros/enum_with_string_conversions.hh>

namespace sim {

ENUM_WITH_STRING_CONVERSIONS(ExecutionFormat, uint8_t,
    (Legacy, 0, "legacy")
    (Communication, 1, "communication")
);

} // namespace sim

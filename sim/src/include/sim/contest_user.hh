#pragma once

#include <simlib/meta.hh>
#include <simlib/mysql.hh>

namespace sim {

struct ContestUser {
    struct Id {
        uintmax_t user_id;
        uintmax_t contest_id;
    };

    enum class Mode : uint8_t { CONTESTANT = 0, MODERATOR = 1, OWNER = 2 };

    Id id;
    EnumVal<Mode> mode;
};

} // namespace sim

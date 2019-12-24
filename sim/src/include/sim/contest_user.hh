#pragma once

#include <simlib/meta.h>
#include <simlib/mysql.h>

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

#define CU_MODE_CONTESTANT_STR "0"
static_assert(
   meta::equal(CU_MODE_CONTESTANT_STR,
               meta::ToString<(int)ContestUser::Mode::CONTESTANT>::value),
   "Update the above #define");

#define CU_MODE_MODERATOR_STR "1"
static_assert(
   meta::equal(CU_MODE_MODERATOR_STR,
               meta::ToString<(int)ContestUser::Mode::MODERATOR>::value),
   "Update the above #define");

#define CU_MODE_OWNER_STR "2"
static_assert(meta::equal(CU_MODE_OWNER_STR,
                          meta::ToString<(int)ContestUser::Mode::OWNER>::value),
              "Update the above #define");

} // namespace sim

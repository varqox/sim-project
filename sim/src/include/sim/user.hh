#pragma once

#include "varchar_field.hh"

#include <simlib/meta.h>
#include <simlib/mysql.h>
#include <simlib/string.h>

namespace sim {

struct User {
	enum class Type : uint8_t { ADMIN = 0, TEACHER = 1, NORMAL = 2 };

	uintmax_t id;
	VarcharField<30> username;
	VarcharField<60> first_name;
	VarcharField<60> last_name;
	VarcharField<60> email;
	VarcharField<64> salt;
	VarcharField<128> password;
	EnumVal<Type> type;
};

#define SIM_ROOT_UID "1"

#define UTYPE_ADMIN_STR "0"
#define UTYPE_TEACHER_STR "1"
#define UTYPE_NORMAL_STR "2"

static_assert(meta::equal(UTYPE_ADMIN_STR,
                          meta::ToString<(int)User::Type::ADMIN>::value),
              "Update the above #define");
static_assert(meta::equal(UTYPE_TEACHER_STR,
                          meta::ToString<(int)User::Type::TEACHER>::value),
              "Update the above #define");
static_assert(meta::equal(UTYPE_NORMAL_STR,
                          meta::ToString<(int)User::Type::NORMAL>::value),
              "Update the above #define");

} // namespace sim

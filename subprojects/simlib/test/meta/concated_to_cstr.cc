#include <simlib/meta/concated_to_cstr.hh>
#include <string_view>

constexpr inline char s1[] = "abc";
constexpr inline char s2[] = " ";
constexpr inline char s3[] = "xyz";

static_assert(std::string_view{meta::concated_to_cstr<>} == "");
static_assert(std::string_view{meta::concated_to_cstr<s1>} == "abc");
static_assert(std::string_view{meta::concated_to_cstr<s1, s2>} == "abc ");
static_assert(std::string_view{meta::concated_to_cstr<s1, s2, s3>} == "abc xyz");

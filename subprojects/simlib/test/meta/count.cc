#include <simlib/meta/count.hh>
#include <string_view>

using std::string_view;

static_assert(meta::count(string_view{"\0", 1}, '\0') == 1);

static_assert(meta::count(string_view{"abbabab"}, 'a') == 3);
static_assert(meta::count(string_view{"abbabab"}, 'b') == 4);
static_assert(meta::count(string_view{"abbabab"}, 'c') == 0);
static_assert(meta::count(string_view{"abbabab"}, '\0') == 0);

constexpr inline char s[] = "abbabab";
static_assert(meta::count(s, 'a') == 3);
static_assert(meta::count(s, 'b') == 4);
static_assert(meta::count(s, 'c') == 0);
static_assert(meta::count(s, '\0') == 0);

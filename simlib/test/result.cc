#include <simlib/result.hh>
#include <string_view>

using std::string_view;
using std::string_view_literals::operator""sv;

static_assert(Ok{"xyz"} == Ok{string_view{"xyz"}});

static_assert(Err{"xyz"} == Err{string_view{"xyz"}});

static_assert(Result<void, char>{Ok{}}.is_ok());
static_assert(!Result<void, char>{Err{'a'}}.is_ok());

static_assert(Result<void, char>{Err{'a'}}.is_err());
static_assert(!Result<void, char>{Ok{}}.is_err());

struct Noncopyable {
    Noncopyable() = default;
    Noncopyable(const Noncopyable&) = delete;
    Noncopyable(Noncopyable&&) = default;
    Noncopyable& operator=(const Noncopyable&) = delete;
    Noncopyable& operator=(Noncopyable&&) = default;
    ~Noncopyable() = default;

    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    [[nodiscard]] constexpr bool to_bool() const noexcept { return true; }
};

static_assert(Result<Noncopyable, char>{Ok{Noncopyable{}}}.unwrap().to_bool());
static_assert(Result<char, Noncopyable>{Err{Noncopyable{}}}.unwrap_err().to_bool());

static_assert((Result<void, int>{Ok{}}.unwrap(), true));
static_assert((Result<int, void>{Err{}}.unwrap_err(), true));

static_assert(Result<double, char>{Ok{3.14}} == Ok{3.14});
static_assert(!(Result<double, char>{Ok{3.14}} == Ok{3.13}));
static_assert(!(Result<double, double>{Ok{3.14}} == Err{3.14}));
static_assert(Result<double, char>{Err{'a'}} == Err{'a'});
static_assert(!(Result<double, char>{Err{'a'}} == Err{'b'}));
static_assert(!(Result<double, double>{Err{3.14}} == Ok{3.14}));

static_assert(!(Result<double, char>{Ok{3.14}} != Ok{3.14}));
static_assert(Result<double, char>{Ok{3.14}} != Ok{3.13});
static_assert(Result<double, double>{Ok{3.14}} != Err{3.14});
static_assert(!(Result<double, char>{Err{'a'}} != Err{'a'}));
static_assert(Result<double, char>{Err{'a'}} != Err{'b'});
static_assert(Result<double, double>{Err{3.14}} != Ok{3.14});

static_assert(Ok{3.14} == Result<double, char>{Ok{3.14}});
static_assert(!(Ok{3.13} == Result<double, char>{Ok{3.14}}));
static_assert(!(Err{3.14} == Result<double, double>{Ok{3.14}}));
static_assert(Err{'a'} == Result<double, char>{Err{'a'}});
static_assert(!(Err{'b'} == Result<double, char>{Err{'a'}}));
static_assert(!(Ok{3.14} == Result<double, double>{Err{3.14}}));

static_assert(!(Ok{3.14} != Result<double, char>{Ok{3.14}}));
static_assert(Ok{3.13} != Result<double, char>{Ok{3.14}});
static_assert(Err{3.14} != Result<double, double>{Ok{3.14}});
static_assert(!(Err{'a'} != Result<double, char>{Err{'a'}}));
static_assert(Err{'b'} != Result<double, char>{Err{'a'}});
static_assert(Ok{3.14} != Result<double, double>{Err{3.14}});

static_assert(Result<double, double>{Ok{3.14}} == Result<double, double>{Ok{3.14}});
static_assert(Result<double, double>{Err{3.14}} == Result<double, double>{Err{3.14}});
static_assert(!(Result<double, double>{Ok{3.14}} == Result<double, double>{Ok{3.13}}));
static_assert(!(Result<double, double>{Err{3.14}} == Result<double, double>{Err{3.13}}));
static_assert(!(Result<double, double>{Ok{3.14}} == Result<double, double>{Err{3.14}}));
static_assert(!(Result<double, double>{Err{3.14}} == Result<double, double>{Ok{3.14}}));

static_assert(!(Result<double, double>{Ok{3.14}} != Result<double, double>{Ok{3.14}}));
static_assert(!(Result<double, double>{Err{3.14}} != Result<double, double>{Err{3.14}}));
static_assert(Result<double, double>{Ok{3.14}} != Result<double, double>{Ok{3.13}});
static_assert(Result<double, double>{Err{3.14}} != Result<double, double>{Err{3.13}});
static_assert(Result<double, double>{Ok{3.14}} != Result<double, double>{Err{3.14}});
static_assert(Result<double, double>{Err{3.14}} != Result<double, double>{Ok{3.14}});

static_assert(Result<double, const char*>{Ok{42.0}} == Result<int, string_view>{Ok{42}});
static_assert(Result<double, const char*>{Err{"abc"}} == Result<int, string_view>{Err{"abc"sv}});
static_assert(!(Result<double, const char*>{Ok{42.1}} == Result<int, string_view>{Ok{42}}));
static_assert(!(Result<double, const char*>{Err{"abcd"}} == Result<int, string_view>{Err{"abc"sv}})
);
static_assert(!(Result<double, const char*>{Ok{42.0}} == Result<int, string_view>{Err{""sv}}));
static_assert(!(Result<double, const char*>{Err{"abc"}} == Result<int, string_view>{Ok{42}}));

static_assert(!(Result<double, const char*>{Ok{42.0}} != Result<int, string_view>{Ok{42}}));
static_assert(!(Result<double, const char*>{Err{"abc"}} != Result<int, string_view>{Err{"abc"sv}}));
static_assert(Result<double, const char*>{Ok{42.1}} != Result<int, string_view>{Ok{42}});
static_assert(Result<double, const char*>{Err{"abcd"}} != Result<int, string_view>{Err{"abc"sv}});
static_assert(Result<double, const char*>{Ok{42.0}} != Result<int, string_view>{Err{""sv}});
static_assert(Result<double, const char*>{Err{"abc"}} != Result<int, string_view>{Ok{42}});

static_assert(Ok{} == Ok<void>{});
static_assert(Ok<void>{} == Ok<void>{});
static_assert(Ok{1} != Ok<void>{});
static_assert(Ok{} != Ok{1});
static_assert(Ok{1} == Ok{1});

static_assert(Result<void, string_view>{Ok{}} == Ok<void>{});
static_assert(Result<void, string_view>{Err{"abc"}} == Err{"abc"});

static_assert(Err{} == Err<void>{});
static_assert(Err<void>{} == Err<void>{});
static_assert(Err{1} != Err<void>{});
static_assert(Err{} != Err{1});
static_assert(Err{1} == Err{1});

static_assert(Result<string_view, void>{Err{}} == Err<void>{});
static_assert(Result<string_view, void>{Ok{"abc"}} == Ok{"abc"});

#include <cctype>
#include <simlib/file_contents.hh>
#include <simlib/string_transform.hh>
#include <simlib/string_view.hh>
#include <simlib/throw_assert.hh>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char** argv) {
    throw_assert(argc == 3);
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    auto expected_uid = str2num<uid_t>(argv[1]).value();
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    auto expected_gid = str2num<uid_t>(argv[2]).value();
    throw_assert(getuid() == expected_uid);
    throw_assert(getgid() == expected_gid);

    struct stat64 st = {};
    throw_assert(stat64("/proc/self/", &st) == 0);
    throw_assert(st.st_uid == expected_uid);
    throw_assert(st.st_gid == expected_gid);

    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    auto overflow_uid =
        str2num<uid_t>(StringView{from_unsafe{get_file_contents("/proc/sys/kernel/overflowuid")}}
                           .without_trailing(isspace))
            .value();
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    auto overflow_gid =
        str2num<uid_t>(StringView{from_unsafe{get_file_contents("/proc/sys/kernel/overflowgid")}}
                           .without_trailing(isspace))
            .value();
    throw_assert(stat64("/proc/", &st) == 0);
    if (expected_uid != 0) {
        throw_assert(st.st_uid == overflow_uid);
    }
    if (expected_gid != 0) {
        throw_assert(st.st_gid == overflow_gid);
    }
}

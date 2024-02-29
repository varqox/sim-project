#include "../gtest_with_tester.hh"
#include "assert_result.hh"

#include <optional>
#include <simlib/concat_tostr.hh>
#include <simlib/random.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/to_string.hh>
#include <string_view>
#include <unistd.h>

using std::literals::string_view_literals::operator""sv;

// NOLINTNEXTLINE
TEST(sandbox, test_uids_and_gids) {
    auto sc = sandbox::spawn_supervisor();
    auto euid = geteuid();
    auto euid_str = to_string(euid);
    auto egid = getegid();
    auto egid_str = to_string(egid);

    // no uid, no gid
    ASSERT_RESULT_OK(
        sc.await_result(sc.send_request(
            {{tester_executable_path, euid_str, egid_str}}, {.stderr_fd = STDERR_FILENO}
        )),
        CLD_EXITED,
        0
    );

    auto options = std::array<std::optional<int>, 4>{{
        std::nullopt,
        0,
        egid,
        get_random(1, 0x8000),
    }};
    for (auto uid : options) {
        for (auto gid : options) {
            auto uid_str = to_string(uid.value_or(euid));
            auto gid_str = to_string(gid.value_or(egid));
            ASSERT_RESULT_OK(
                sc.await_result(sc.send_request(
                    {{tester_executable_path, uid_str, gid_str}},
                    {
                        .linux_namespaces =
                            {
                                .user =
                                    {
                                        .inside_uid = uid,
                                        .inside_gid = gid,
                                    },
                            },
                    }
                )),
                CLD_EXITED,
                0
            ) << "  uid: "
              << (uid.has_value() ? uid_str : "nullopt"sv)
              << "\n  gid: " << (gid.has_value() ? gid_str : "nullopt"sv);
        }
    }
}

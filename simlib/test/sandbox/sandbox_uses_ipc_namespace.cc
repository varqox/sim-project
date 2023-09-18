#include "../gtest_with_tester.hh"
#include "assert_result.hh"

#include <fcntl.h>
#include <mqueue.h>
#include <simlib/defer.hh>
#include <simlib/file_perms.hh>
#include <simlib/random.hh>
#include <simlib/sandbox/sandbox.hh>
#include <unistd.h>

// NOLINTNEXTLINE
TEST(sandbox, sandbox_uses_net_namespace) {
    auto sc = sandbox::spawn_supervisor();
    std::string name = "/simlib_sandbox_uses_ipc_namespace_test.";
    for (;;) {
        auto saved_size = name.size();
        for (int i = 0; i < 16; ++i) {
            name += get_random('a', 'z');
        }
        auto mq_id = mq_open(name.c_str(), O_RDWR | O_CLOEXEC | O_CREAT | O_EXCL, S_0755, nullptr);
        if (mq_id == -1 and errno == EEXIST) {
            name.resize(saved_size);
            continue;
        }
        ASSERT_NE(mq_id, -1);
        ASSERT_EQ(mq_close(mq_id), 0);
        break;
    }
    Defer mq_unlinker = [&] { (void)mq_unlink(name.c_str()); };
    ASSERT_RESULT_OK(
        sc.await_result(
            sc.send_request({{tester_executable_path, name}}, {.stderr_fd = STDERR_FILENO})
        ),
        CLD_EXITED,
        0
    );
}

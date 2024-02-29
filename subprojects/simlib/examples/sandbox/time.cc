#include <array>
#include <csignal>
#include <iostream>
#include <poll.h>
#include <simlib/file_descriptor.hh>
#include <simlib/humanize.hh>
#include <simlib/overloaded.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/throw_assert.hh>
#include <simlib/time_format_conversions.hh>
#include <simlib/to_arg_seq.hh>
#include <string_view>
#include <sys/signalfd.h>
#include <unistd.h>

int main(int argc, char** argv) {
    // Block signals (also in sandbox supervisor) so we can kill request upon them
    sigset_t ss;
    throw_assert(sigemptyset(&ss) == 0);
    throw_assert(sigaddset(&ss, SIGINT) == 0);
    throw_assert(sigaddset(&ss, SIGQUIT) == 0);
    throw_assert(sigaddset(&ss, SIGTERM) == 0);
    throw_assert(pthread_sigmask(SIG_BLOCK, &ss, nullptr) == 0);

    auto sb = sandbox::spawn_supervisor();
    std::vector<std::string_view> sargv;
    sargv.reserve(argc - 1);
    for (auto arg : to_arg_seq(argc, argv)) {
        sargv.emplace_back(arg);
    }
    auto rh = sb.send_request(
        sargv,
        {
            .stdin_fd = STDIN_FILENO,
            .stdout_fd = STDOUT_FILENO,
            .stderr_fd = STDERR_FILENO,
            .env =
                [&] {
                    std::vector<std::string_view> env;
                    for (auto e = environ; *e; ++e) {
                        env.emplace_back(*e);
                    }
                    return env;
                }(),
        }
    );

    auto krh = rh.get_kill_handle();
    // Wait for signal or request completion
    auto sfd = FileDescriptor{signalfd(-1, &ss, SFD_CLOEXEC)};
    throw_assert(sfd.is_open());
    std::array<pollfd, 2> pfds;

    enum {
        REQUEST = 0,
        SIGNAL = 1,
    };

    pfds[REQUEST] = {
        .fd = rh.pollable_fd(),
        .events = POLLIN,
        .revents = 0,
    };
    pfds[SIGNAL] = {
        .fd = sfd,
        .events = POLLIN,
        .revents = 0,
    };
    for (;;) {
        int rc = poll(pfds.data(), pfds.size(), -1);
        if (rc == 0 || (rc == -1 && errno == EINTR)) {
            continue;
        }

        if (pfds[SIGNAL].events & POLLIN) {
            krh.kill();
        }
        break;
    }

    auto res = sb.await_result(std::move(rh));
    std::visit(
        overloaded{
            [&](const sandbox::result::Ok& res_ok) {
                std::cout << res_ok.si.description() << '\n';
                std::cout << "real time: " << to_string(res_ok.runtime, false).c_str() << '\n';
                std::cout << "cpu time:  "
                          << to_string(res_ok.cgroup.cpu_time.total(), false).c_str() << '\n';
                std::cout << "  user:    " << to_string(res_ok.cgroup.cpu_time.user, false).c_str()
                          << '\n';
                std::cout << "  system:  "
                          << to_string(res_ok.cgroup.cpu_time.system, false).c_str() << '\n';
                std::cout << "peak memory: " << res_ok.cgroup.peak_memory_in_bytes
                          << " bytes = " << humanize_file_size(res_ok.cgroup.peak_memory_in_bytes)
                          << std::endl;
            },
            [&](const sandbox::result::Error& res_err) {
                std::cout << "Error: " << res_err.description << std::endl;
            },
        },
        res
    );
}

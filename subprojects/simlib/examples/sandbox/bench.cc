#include <chrono>
#include <iostream>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/throw_assert.hh>
#include <simlib/time_format_conversions.hh>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

int main() {
    auto sb = sandbox::spawn_supervisor();

    constexpr auto N = 300;
    auto benchmark = [&](auto name, auto run) {
        auto start_tp = std::chrono::steady_clock::now();
        run();
        auto duration = (std::chrono::steady_clock::now() - start_tp) / N;
        std::cout << name << ": " << to_string(duration, false).c_str() << " = "
                  << std::chrono::seconds{1} / duration << " runs / second" << std::endl;
    };

    benchmark("  no sandbox", [&] {
        for (size_t i = 0; i < N; ++i) {
            pid_t pid = fork();
            throw_assert(pid != -1);
            if (pid == 0) {
                char* empty[] = {nullptr};
                execve("/bin/true", empty, empty);
                _exit(1);
            }
            int status;
            throw_assert(waitpid(pid, &status, 0) == pid);
            throw_assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
        }
    });

    // Ignore a few first runs
    for (size_t i = 0; i < N / 10; i++) {
        sb.await_result(sb.send_request({{"/bin/true"}}, {}));
    }

    benchmark(" synchronous", [&] {
        for (size_t i = 0; i < N; i++) {
            sb.await_result(sb.send_request({{"/bin/true"}}, {}));
        }
    });
    benchmark("asynchronous", [&] {
        std::vector<sandbox::SupervisorConnection::RequestHandle> handles;
        handles.reserve(N);
        for (size_t i = 0; i < N; i++) {
            handles.emplace_back(sb.send_request({{"/bin/true"}}, {}));
        }
        for (size_t i = 0; i < N; i++) {
            sb.await_result(std::move(handles[i]));
        }
    });
}

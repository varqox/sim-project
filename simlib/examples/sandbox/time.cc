#include <iostream>
#include <simlib/humanize.hh>
#include <simlib/overloaded.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/time_format_conversions.hh>
#include <simlib/to_arg_seq.hh>
#include <string_view>
#include <unistd.h>

int main(int argc, char** argv) {
    auto sb = sandbox::spawn_supervisor();
    std::vector<std::string_view> sargv;
    sargv.reserve(argc - 1);
    for (auto arg : to_arg_seq(argc, argv)) {
        sargv.emplace_back(arg);
    }
    auto res = sb.await_result(sb.send_request(
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
    ));
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

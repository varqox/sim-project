#include "../logs.hh"
#include "../old/sim.hh"
#include "connection.hh"

#include <arpa/inet.h>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <netinet/in.h>
#include <pthread.h>
#include <simlib/am_i_root.hh>
#include <simlib/config_file.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/process.hh>
#include <simlib/time.hh>
#include <simlib/time_format_conversions.hh>
#include <simlib/working_directory.hh>
#include <thread>

using std::string;

namespace web_server::server {

static void* worker(void* ptr) {
    int socket_fd = reinterpret_cast<intptr_t>(ptr);
    try {
        sockaddr_in name{};
        socklen_t client_name_len = sizeof(name);
        char ip[INET_ADDRSTRLEN];
        Connection conn(-1);
        old::Sim sim_worker;

        for (;;) {
            // accept the connection
            FileDescriptor client_socket_fd{accept4(
                socket_fd, reinterpret_cast<sockaddr*>(&name), &client_name_len, SOCK_CLOEXEC
            )};
            if (client_socket_fd == -1) {
                continue;
            }

            // extract IP
            inet_ntop(AF_INET, &name.sin_addr, ip, INET_ADDRSTRLEN);
            stdlog("Connection accepted: ", pthread_self(), " form ", ip);

            conn.assign(client_socket_fd);
            http::Request req = conn.get_request();

            if (conn.state() == Connection::OK) {
                using std::chrono::steady_clock;
                auto beg = steady_clock::now();

                http::Response resp = sim_worker.handle(std::move(req));

                auto microdur = std::chrono::duration_cast<std::chrono::microseconds>(
                    steady_clock::now() - beg
                );
                stdlog("Response generated in ", to_string(microdur * 1000), " ms.");

                conn.send_response(resp);
            }

            stdlog("Closing...");
            (void)client_socket_fd.close();
            stdlog("Closed");
        }

    } catch (const std::exception& e) {
        ERRLOG_CATCH(e);

    } catch (...) {
        ERRLOG_CATCH();
    }

    return nullptr;
}

} // namespace web_server::server

int main() {
    // Init server
    // Change directory to process executable directory
    try {
        chdir_relative_to_executable_dirpath("..");
    } catch (const std::exception& e) {
        errlog("Failed to change the working directory: ", e.what());
        return 1;
    }

    if (am_i_root() != AmIRoot::NO) {
        errlog("This program should not be run as root.");
        return 1;
    }

    // Terminate older instances
    kill_processes_by_exec({executable_path(getpid())}, std::chrono::seconds(4), true);

    // Loggers
    if (freopen(web_server::stdlog_file.data(), "a", stdout) == nullptr) {
        errlog("Failed to open: ", web_server::stdlog_file, errmsg());
    }
    stdlog.use(stdout);

    if (freopen(web_server::errlog_file.data(), "a", stderr) == nullptr) {
        errlog("Failed to open: ", web_server::errlog_file, errmsg());
    }
    errlog.use(stderr);

    // Signal control
    struct sigaction sa = {};
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &exit;

    (void)sigaction(SIGINT, &sa, nullptr);
    (void)sigaction(SIGTERM, &sa, nullptr);
    (void)sigaction(SIGQUIT, &sa, nullptr);

    ConfigFile config;
    try {
        config.add_vars("web_server_address", "web_server_workers");

        config.load_config_from_file("sim.conf");
    } catch (const std::exception& e) {
        errlog("Failed to load sim.config: ", e.what());
        return 5;
    }

    string full_address = config["web_server_address"].as_string();
    auto workers = config["web_server_workers"].as<size_t>().value_or(0);

    if (workers < 1) {
        errlog("sim.conf: Number of web_server_workers has to be an integer greater than 0");
        return 6;
    }

    sockaddr_in name{};
    name.sin_family = AF_INET;
    memset(name.sin_zero, 0, sizeof(name.sin_zero));

    // Extract port from address
    in_port_t port = 80; // server port
    CStringView address_str;
    if (size_t colon_pos = full_address.find(':'); colon_pos != std::string::npos) {
        full_address[colon_pos] = '\0';
        address_str = CStringView(full_address.data(), colon_pos);

        auto port_opt = str2num<decltype(port)>(substring(full_address, colon_pos + 1));
        if (not port_opt) {
            errlog("sim.config: incorrect port number");
            return 7;
        }

        port = *port_opt;
    } else {
        address_str = full_address;
    }

    // Set server port
    name.sin_port = htons(port);

    // Extract IPv4 address
    if (address_str == "*") {
        name.sin_addr.s_addr = htonl(INADDR_ANY); // server address
    } else if (address_str.empty() || inet_aton(address_str.data(), &name.sin_addr) == 0) {
        errlog("sim.config: incorrect IPv4 address");
        return 8;
    }

    // clang-format off
    stdlog("\n=================== Server launched ==================="
           "\nPID: ", getpid(),
           "\nworkers: ", workers,
           "\naddress: ", address_str, ':', port);
    // clang-format on

    int socket_fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
    if (socket_fd < 0) {
        errlog("Failed to create socket", errmsg());
        return 1;
    }

    int true_ = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &true_, sizeof(int))) {
        errlog("Failed to setopt", errmsg());
        return 2;
    }

    // Bind
    constexpr int FAST_SILENT_TRIES = 40;
    constexpr int SLOW_TRIES = 8;
    int bound = [&] {
        auto call_bind = [&] {
            return bind(socket_fd, reinterpret_cast<sockaddr*>(&name), sizeof(name));
        };
        for (int try_no = 1; try_no <= FAST_SILENT_TRIES; ++try_no) {
            if (try_no > 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000 / FAST_SILENT_TRIES));
            }

            if (call_bind() == 0) {
                return true;
            }
        }

        for (int try_no = 1; try_no <= SLOW_TRIES; ++try_no) {
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
            if (call_bind() == 0) {
                return true;
            }

            errlog("Failed to bind (try ", try_no, ')', errmsg());
        }

        return false;
    }();

    if (not bound) {
        errlog("Giving up");
        return 3;
    }

    if (listen(socket_fd, 10)) {
        errlog("Failed to listen", errmsg());
        return 4;
    }

    // Alter default thread stack size
    pthread_attr_t attr;
    constexpr size_t THREAD_STACK_SIZE = 4 << 20; // 4 MiB
    if (pthread_attr_init(&attr) || pthread_attr_setstacksize(&attr, THREAD_STACK_SIZE)) {
        errlog("Failed to set new thread stack size");
        return 4;
    }

    std::vector<pthread_t> threads(workers);
    for (size_t i = 1; i < workers; ++i) {
        pthread_create(
            &threads[i],
            &attr,
            web_server::server::worker,
            reinterpret_cast<void*>(socket_fd)
        ); // TODO: errors...
    }
    threads[0] = pthread_self();
    web_server::server::worker(reinterpret_cast<void*>(socket_fd));

    close(socket_fd);
    return 0;
}

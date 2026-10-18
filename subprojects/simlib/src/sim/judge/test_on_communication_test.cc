#include "get_checker_output_report.hh"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <csignal>
#include <fcntl.h>
#include <limits>
#include <optional>
#include <poll.h>
#include <simlib/concat_tostr.hh>
#include <simlib/defer.hh>
#include <simlib/errmsg.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/macros/throw.hh>
#include <simlib/pipe.hh>
#include <simlib/sim/judge/communication_protocol.hh>
#include <simlib/sim/judge/test_on_communication_test.hh>
#include <simlib/unlinked_temporary_file.hh>
#include <simlib/working_directory.hh>
#include <string>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

using BindMount = sandbox::RequestOptions::LinuxNamespaces::Mount::BindMount;
using CreateFile = sandbox::RequestOptions::LinuxNamespaces::Mount::CreateFile;

namespace sim::judge {
namespace {

using Status = TestReport::Status;
using Clock = std::chrono::steady_clock;
using communication_protocol::Command;
using communication_protocol::Event;
using communication_protocol::MAX_PAYLOAD_SIZE;

struct Spool {
    FileDescriptor fd = open_unlinked_tmp_file();
    uint64_t size = 0;
    uint64_t consumed = 0;

    Spool() {
        if (not fd.is_open()) {
            THROW("open_unlinked_tmp_file()", errmsg());
        }
    }

    void append(std::string_view data) {
        while (not data.empty()) {
            auto rc = pwrite(fd, data.data(), data.size(), static_cast<off_t>(size));
            if (rc < 0 && errno == EINTR) {
                continue;
            }
            if (rc <= 0) {
                THROW("pwrite()", errmsg());
            }
            size += rc;
            data.remove_prefix(rc);
        }
    }

    size_t read(char* data, size_t capacity, uint64_t offset) const {
        for (;;) {
            auto rc = pread(fd, data, capacity, static_cast<off_t>(offset));
            if (rc < 0 && errno == EINTR) {
                continue;
            }
            if (rc < 0) {
                THROW("pread()", errmsg());
            }
            return rc;
        }
    }
};

Pipe make_pipe() {
    auto pipe = pipe2(O_CLOEXEC);
    if (not pipe) {
        THROW("pipe2()", errmsg());
    }
    return std::move(*pipe);
}

void make_nonblocking(int fd) {
    auto flags = fcntl(fd, F_GETFL);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK)) {
        THROW("fcntl()", errmsg());
    }
}

bool ready(int fd) {
    if (fd < 0) {
        return false;
    }
    pollfd pfd{.fd = fd, .events = POLLIN, .revents = 0};
    for (;;) {
        auto rc = poll(&pfd, 1, 0);
        if (rc < 0 && errno == EINTR) {
            continue;
        }
        if (rc < 0) {
            THROW("poll()", errmsg());
        }
        return rc > 0;
    }
}

struct Instance {
    bool batch;
    Spool input;
    std::optional<Spool> output;
    FileDescriptor stdin_fd;
    FileDescriptor stdout_fd;
    std::optional<language_suite::Suite::Execution> execution;
    std::optional<sandbox::result::Ok> result;
    Status status = Status::OK;
    uint64_t output_size = 0;
    uint64_t published = 0;
    bool input_closed = false;
    bool output_closed = false;
    bool completed = false;
    bool cancelled = false;
    bool launched = false;
    bool started = false;
    Clock::time_point startup_deadline;

    explicit Instance(bool batch_) : batch{batch_} {
        if (batch) {
            output.emplace();
        }
    }
};

struct Failure {
    Status status;
    std::string comment;
};

} // namespace

TestReport test_on_communication_test(CommunicationTestArgs args) {
    using namespace std::chrono_literals;

    static const auto page_size = static_cast<uint64_t>(sysconf(_SC_PAGESIZE));
    auto min_instances = args.min_instances.value_or(args.instances);
    if (args.instances == 0 || args.instances > CommunicationConfig::max_instances ||
        min_instances == 0 || min_instances > args.instances || args.max_concurrent == 0 ||
        args.max_concurrent > args.instances ||
        args.max_concurrent > CommunicationConfig::max_concurrent_instances ||
        args.program.time_limit < 0ns || args.program.cpu_time_limit < 0ns ||
        args.manager.time_limit < 0ns || args.manager.cpu_time_limit < 0ns ||
        args.limits.startup_time <= 0ns || args.program.memory_limit_in_bytes == 0 ||
        args.manager.memory_limit_in_bytes == 0 || args.limits.memory_in_bytes < (16 << 20) ||
        args.limits.session_data_in_bytes >
            static_cast<uint64_t>(std::numeric_limits<off_t>::max()) ||
        args.manager.max_comment_len > (1 << 20))
    {
        THROW("Invalid communication test limits");
    }
    CommunicationConfig::validate_memory_limits(
        args.max_concurrent,
        args.program.memory_limit_in_bytes,
        args.manager.memory_limit_in_bytes,
        args.limits.memory_in_bytes
    );
    auto max_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(24h);
    if (args.program.time_limit > max_duration / args.instances ||
        args.manager.time_limit > max_duration ||
        args.limits.startup_time > max_duration / (args.instances + 1))
    {
        THROW("Communication session time limit is too large");
    }
    auto session_time = args.program.time_limit * args.instances + args.manager.time_limit +
        args.limits.startup_time * (args.instances + 1);

    sigset_t sigpipe_set;
    sigemptyset(&sigpipe_set);
    sigaddset(&sigpipe_set, SIGPIPE);
    sigset_t old_sigmask;
    if (pthread_sigmask(SIG_BLOCK, &sigpipe_set, &old_sigmask)) {
        THROW("pthread_sigmask()");
    }
    sigset_t pending;
    if (sigpending(&pending)) {
        (void)pthread_sigmask(SIG_SETMASK, &old_sigmask, nullptr);
        THROW("sigpending()", errmsg());
    }
    bool had_sigpipe = sigismember(&pending, SIGPIPE);
    bool encountered_sigpipe = false;
    Defer restore_sigmask = [&] {
        if (encountered_sigpipe && not had_sigpipe) {
            timespec timeout{};
            while (sigtimedwait(&sigpipe_set, nullptr, &timeout) < 0 && errno == EINTR) {
            }
        }
        (void)pthread_sigmask(SIG_SETMASK, &old_sigmask, nullptr);
    };

    auto program = args.compiled_program.prepared_program();
    auto manager_program = args.compiled_manager.prepared_program();
    auto started = Clock::now();
    auto deadline = started + session_time;
    auto manager_stdin = make_pipe();
    auto manager_stdout = make_pipe();
    make_nonblocking(manager_stdin.writable);
    make_nonblocking(manager_stdout.readable);
    auto manager_report_fd =
        FileDescriptor{memfd_create("communication manager result", MFD_CLOEXEC)};
    if (not manager_report_fd.is_open()) {
        THROW("memfd_create()", errmsg());
    }
    auto manager = manager_program.start_fresh(
        {{"--sim-communication-v1", "/in"}},
        {
            .stdin_fd = manager_stdin.readable,
            .stdout_fd = manager_stdout.writable,
            .stderr_fd = manager_report_fd,
            .time_limit = session_time + args.limits.startup_time,
            .cpu_time_limit = args.manager.cpu_time_limit,
            .memory_limit_in_bytes = args.manager.memory_limit_in_bytes + page_size,
            .max_stack_size_in_bytes = args.manager.memory_limit_in_bytes + page_size,
            .max_file_size_in_bytes = args.manager.max_comment_len + 32,
            .process_num_limit = 1,
            .rootfs = {.inode_limit = 1},
            .env = {},
        },
        {{
            CreateFile{.path = "/../in"},
            BindMount{
                .source = has_prefix(StringView{args.test_input}, "/")
                    ? std::string_view{args.test_input}
                    : std::string_view{concat_tostr(get_cwd(), args.test_input)},
                .dest = "/../in",
            },
        }}
    );
    (void)manager_stdin.readable.close();
    (void)manager_stdout.writable.close();

    std::vector<Instance> instances;
    instances.reserve(args.instances);
    Spool events;
    uint64_t stored_bytes = 0;
    uint64_t received_bytes = 0;
    std::string commands;
    std::optional<Failure> manager_failure;
    std::optional<Failure> program_failure;
    uint32_t program_failure_id = 0;
    std::optional<sandbox::result::Ok> manager_result;
    std::optional<bool> finish;
    bool hello = false;
    bool manager_started = false;
    auto manager_startup_deadline = started + args.limits.startup_time;
    uint32_t running = 0;
    std::array<char, MAX_PAYLOAD_SIZE> buffer;

    auto fail_manager = [&](std::string comment) {
        if (not manager_failure) {
            manager_failure = Failure{Status::CheckerError, "Manager error: " + std::move(comment)};
        }
    };
    auto fail_program = [&](uint32_t id, Status status, std::string comment) {
        if (not program_failure || id < program_failure_id ||
            (id == program_failure_id && program_failure->status == Status::RuntimeError &&
             status == Status::OutputSizeLimitExceeded))
        {
            program_failure =
                Failure{status, concat_tostr("Instance ", id, ": ", std::move(comment))};
            program_failure_id = id;
        }
    };
    auto reserve_storage = [&](uint64_t size) {
        if (size > args.limits.session_data_in_bytes - stored_bytes) {
            return false;
        }
        stored_bytes += size;
        return true;
    };
    auto event = [&](Event type, uint32_t id, std::string_view data = {}) {
        if (not reserve_storage(12 + data.size())) {
            if (type == Event::Output) {
                instances[id - 1].status = Status::OutputSizeLimitExceeded;
                fail_program(id, Status::OutputSizeLimitExceeded, "session data limit exceeded");
            } else {
                fail_manager("session data limit exceeded");
            }
            return;
        }
        auto header = communication_protocol::header(static_cast<uint32_t>(type), id, data.size());
        events.append({header.data(), header.size()});
        events.append(data);
    };
    auto write_pipe = [&](Spool& spool, FileDescriptor& fd) {
        if (not fd.is_open() || spool.consumed == spool.size) {
            return;
        }
        auto size = spool.read(buffer.data(), buffer.size(), spool.consumed);
        auto rc = write(fd, buffer.data(), size);
        if (rc > 0) {
            spool.consumed += rc;
        } else if (rc < 0 && errno == EPIPE) {
            encountered_sigpipe = true;
            (void)fd.close();
        } else if (rc < 0 && errno != EINTR && errno != EAGAIN) {
            THROW("write()", errmsg());
        }
    };
    auto launch = [&](uint32_t id) {
        auto& instance = instances[id - 1];
        if (running >= args.max_concurrent) {
            fail_manager("concurrent instance limit exceeded");
            return;
        }
        auto output = make_pipe();
        make_nonblocking(output.readable);
        std::optional<Pipe> input;
        FileDescriptor batch_input;
        if (instance.batch) {
            batch_input = FileDescriptor{
                concat_tostr("/proc/self/fd/", static_cast<int>(instance.input.fd)),
                O_RDONLY | O_CLOEXEC
            };
            if (not batch_input.is_open()) {
                THROW("open()", errmsg());
            }
        } else {
            input.emplace(make_pipe());
            make_nonblocking(input->writable);
        }
        auto remaining = std::max(
            0ns, std::chrono::duration_cast<std::chrono::nanoseconds>(deadline - Clock::now())
        );
        instance.launched = true;
        instance.startup_deadline = Clock::now() + args.limits.startup_time;
        instance.execution.emplace(program.start_fresh(
            {},
            {
                .stdin_fd = instance.batch ? static_cast<int>(batch_input)
                                           : static_cast<int>(input->readable),
                .stdout_fd = output.writable,
                .stderr_fd = std::nullopt,
                .time_limit =
                    instance.batch ? args.program.time_limit : remaining + args.limits.startup_time,
                .cpu_time_limit = args.program.cpu_time_limit,
                .memory_limit_in_bytes = args.program.memory_limit_in_bytes + page_size,
                .max_stack_size_in_bytes = args.program.memory_limit_in_bytes + page_size,
                .max_file_size_in_bytes = 0,
                .process_num_limit = 1,
                .env = {},
            },
            {}
        ));
        instance.stdout_fd = std::move(output.readable);
        if (input) {
            instance.stdin_fd = std::move(input->writable);
        }
        ++running;
    };
    auto process_command = [&](uint32_t type, uint32_t id, std::string_view data) {
        if (finish) {
            fail_manager("command after finish");
            return;
        }
        auto command = static_cast<Command>(type);
        if (not hello) {
            if (command != Command::Hello || id != 0 || data.size() != 4 ||
                communication_protocol::decode(data.data()) != communication_protocol::VERSION)
            {
                fail_manager("expected protocol version 1 handshake");
                return;
            }
            hello = true;
            char response[12];
            communication_protocol::encode(response, communication_protocol::VERSION);
            communication_protocol::encode(response + 4, args.instances);
            communication_protocol::encode(response + 8, args.max_concurrent);
            event(Event::Hello, 0, {response, sizeof(response)});
            return;
        }
        if (command == Command::PrepareBatch || command == Command::Start) {
            if (not data.empty() || id != instances.size() + 1 || id > args.instances) {
                fail_manager("invalid instance reservation");
                return;
            }
            instances.emplace_back(command == Command::PrepareBatch);
            if (command == Command::Start) {
                launch(id);
            }
            return;
        }
        if (command == Command::Finish) {
            if (id != 0 || data.size() != 1 || (data[0] != 0 && data[0] != 1)) {
                fail_manager("invalid finish command");
                return;
            }
            finish = data[0] == 1;
            if (*finish && not program_failure) {
                if (instances.size() < min_instances ||
                    std::any_of(
                        instances.begin(),
                        instances.end(),
                        [](const auto& instance) {
                            return not instance.completed || instance.status != Status::OK;
                        }
                    ))
                {
                    fail_manager("success before all instances completed");
                }
            } else if (not *finish) {
                for (auto& instance : instances) {
                    if (instance.execution && not instance.result) {
                        instance.cancelled = true;
                        instance.execution->kill();
                    }
                }
            }
            return;
        }
        if (id == 0 || id > instances.size()) {
            fail_manager("invalid instance ID");
            return;
        }
        auto& instance = instances[id - 1];
        switch (command) {
        case Command::BatchInput:
        case Command::Input: {
            if (instance.input_closed ||
                (command == Command::BatchInput ? not instance.batch || instance.launched
                                                : instance.batch || not instance.launched))
            {
                fail_manager("input in invalid instance state");
                return;
            }
            if (data.size() > args.limits.input_size_in_bytes - instance.input.size ||
                not reserve_storage(data.size()))
            {
                fail_manager("input size limit exceeded");
                return;
            }
            instance.input.append(data);
        } break;
        case Command::CommitBatch:
            if (not data.empty() || not instance.batch || instance.launched) {
                fail_manager("invalid batch commit");
                return;
            }
            instance.input_closed = true;
            launch(id);
            break;
        case Command::CloseInput:
            if (not data.empty() || instance.batch || not instance.launched ||
                instance.input_closed)
            {
                fail_manager("invalid input close");
                return;
            }
            instance.input_closed = true;
            break;
        default: fail_manager("unknown command"); break;
        }
    };

    auto read_manager = [&] {
        if (not manager_stdout.readable.is_open()) {
            return;
        }
        auto rc = read(manager_stdout.readable, buffer.data(), buffer.size());
        if (rc < 0 && (errno == EAGAIN || errno == EINTR)) {
            return;
        }
        if (rc < 0) {
            THROW("read()", errmsg());
        }
        if (rc == 0) {
            (void)manager_stdout.readable.close();
            if (not commands.empty()) {
                fail_manager("truncated command");
            }
            if (not finish) {
                fail_manager("missing finish command");
            }
            return;
        }
        if (static_cast<uint64_t>(rc) > args.limits.session_data_in_bytes - received_bytes) {
            fail_manager("control data limit exceeded");
            return;
        }
        received_bytes += rc;
        commands.append(buffer.data(), rc);
        size_t offset = 0;
        while (commands.size() - offset >= 12 && not manager_failure) {
            auto type = communication_protocol::decode(commands.data() + offset);
            auto id = communication_protocol::decode(commands.data() + offset + 4);
            auto size = communication_protocol::decode(commands.data() + offset + 8);
            if (size > MAX_PAYLOAD_SIZE) {
                fail_manager("frame size limit exceeded");
                break;
            }
            if (commands.size() - offset - 12 < size) {
                break;
            }
            process_command(type, id, {commands.data() + offset + 12, size});
            offset += 12 + size;
        }
        commands.erase(0, offset);
    };

    auto observe_startup = [&] {
        if (not manager_started) {
            if (ready(manager.started_fd())) {
                manager_started = true;
            } else if (Clock::now() >= manager_startup_deadline && not ready(manager.pollable_fd()))
            {
                THROW("Communication manager startup timed out");
            }
        }
        for (uint32_t i = 0; i < instances.size(); ++i) {
            auto& instance = instances[i];
            if (instance.execution && not instance.started) {
                if (ready(instance.execution->started_fd())) {
                    instance.started = true;
                    event(Event::Started, i + 1);
                } else if (
                    Clock::now() >= instance.startup_deadline &&
                    not ready(instance.execution->pollable_fd())
                )
                {
                    THROW("Communication instance startup timed out");
                }
            }
        }
    };

    while (not manager_failure && not program_failure) {
        std::vector<pollfd> pfds;
        auto add_fd = [&](int fd, short events) {
            if (fd >= 0) {
                pfds.push_back({.fd = fd, .events = events, .revents = 0});
            }
        };
        add_fd(manager_stdout.readable, POLLIN);
        if (events.consumed != events.size) {
            add_fd(manager_stdin.writable, POLLOUT);
        }
        if (not manager_result) {
            add_fd(manager.pollable_fd(), POLLIN);
            if (not manager_started) {
                add_fd(manager.started_fd(), POLLIN);
            }
        }
        for (auto& instance : instances) {
            if (instance.execution && not instance.result) {
                add_fd(instance.execution->pollable_fd(), POLLIN);
                if (not instance.started) {
                    add_fd(instance.execution->started_fd(), POLLIN);
                }
            }
            add_fd(instance.stdout_fd, POLLIN);
            if (instance.input.consumed != instance.input.size) {
                add_fd(instance.stdin_fd, POLLOUT);
            } else if (instance.input_closed) {
                (void)instance.stdin_fd.close();
            }
        }
        auto remaining_ms =
            std::chrono::ceil<std::chrono::milliseconds>(deadline - Clock::now()).count();
        auto rc = poll(
            pfds.data(), pfds.size(), static_cast<int>(std::clamp<int64_t>(remaining_ms, 0, 50))
        );
        if (rc < 0 && errno == EINTR) {
            continue;
        }
        if (rc < 0) {
            THROW("poll()", errmsg());
        }
        observe_startup();
        for (uint32_t i = 0; i < instances.size(); ++i) {
            auto& instance = instances[i];
            if (not instance.execution || instance.result ||
                not ready(instance.execution->pollable_fd()))
            {
                continue;
            }
            instance.result = instance.execution->await_result();
            instance.execution.reset();
            auto& result = *instance.result;
            if (instance.cancelled) {
                continue;
            }
            if (result.cgroup.cpu_time.total() > args.program.cpu_time_limit ||
                (instance.batch && result.runtime > args.program.time_limit))
            {
                instance.status = Status::TimeLimitExceeded;
                fail_program(i + 1, instance.status, "time limit exceeded");
            } else if (result.cgroup.peak_memory_in_bytes > args.program.memory_limit_in_bytes) {
                instance.status = Status::MemoryLimitExceeded;
                fail_program(i + 1, instance.status, "memory limit exceeded");
            } else if (result.si != sandbox::Si{.code = CLD_EXITED, .status = 0}) {
                instance.status = Status::RuntimeError;
                fail_program(i + 1, instance.status, result.si.description());
            }
        }
        if (not manager_result && ready(manager.pollable_fd())) {
            manager_result = manager.await_result();
            if (manager_result->cgroup.cpu_time.total() > args.manager.cpu_time_limit) {
                fail_manager("time limit exceeded");
            } else if (
                manager_result->cgroup.peak_memory_in_bytes > args.manager.memory_limit_in_bytes
            )
            {
                fail_manager("memory limit exceeded");
            } else if (manager_result->si != sandbox::Si{.code = CLD_EXITED, .status = 0}) {
                fail_manager(manager_result->si.description());
            }
        }
        for (uint32_t i = 0; i < instances.size(); ++i) {
            auto& instance = instances[i];
            write_pipe(instance.input, instance.stdin_fd);
            if (instance.stdout_fd.is_open()) {
                auto count = read(instance.stdout_fd, buffer.data(), buffer.size());
                if (count < 0 && errno != EAGAIN && errno != EINTR) {
                    THROW("read()", errmsg());
                }
                if (count == 0) {
                    (void)instance.stdout_fd.close();
                    instance.output_closed = true;
                    if (not instance.batch && not finish) {
                        event(Event::OutputClosed, i + 1);
                    }
                } else if (count > 0 && not instance.cancelled) {
                    if (static_cast<uint64_t>(count) >
                            args.program.output_size_limit_in_bytes - instance.output_size ||
                        (instance.batch && not reserve_storage(count)))
                    {
                        if (instance.status == Status::OK ||
                            instance.status == Status::RuntimeError)
                        {
                            instance.status = Status::OutputSizeLimitExceeded;
                        }
                        fail_program(
                            i + 1, Status::OutputSizeLimitExceeded, "output size limit exceeded"
                        );
                    } else {
                        instance.output_size += count;
                        if (instance.batch) {
                            instance.output->append({buffer.data(), static_cast<size_t>(count)});
                        } else if (not finish) {
                            event(
                                Event::Output, i + 1, {buffer.data(), static_cast<size_t>(count)}
                            );
                        }
                    }
                }
            }
            if (instance.result && instance.output_closed && not instance.completed) {
                if (instance.batch && instance.status == Status::OK && not instance.cancelled &&
                    not finish)
                {
                    auto count =
                        instance.output->read(buffer.data(), buffer.size(), instance.published);
                    if (count) {
                        event(Event::Output, i + 1, {buffer.data(), count});
                        instance.published += count;
                    }
                    if (instance.published != instance.output_size) {
                        continue;
                    }
                    event(Event::OutputClosed, i + 1);
                }
                instance.completed = true;
                --running;
                (void)instance.stdin_fd.close();
                if (not finish && instance.status == Status::OK) {
                    event(Event::Ended, i + 1);
                }
            }
        }
        read_manager();
        write_pipe(events, manager_stdin.writable);
        if (manager_result && not manager_stdout.readable.is_open()) {
            break;
        }
        if (Clock::now() >= deadline && not manager_failure && not program_failure) {
            if (running != 0) {
                program_failure =
                    Failure{Status::TimeLimitExceeded, "Communication session time limit exceeded"};
            } else {
                fail_manager("session time limit exceeded");
            }
        }
    }

    bool manager_cancelled = false;
    if (not manager_result) {
        manager_cancelled = true;
        manager.kill();
    }
    for (auto& instance : instances) {
        if (instance.execution && not instance.result) {
            instance.cancelled = true;
            instance.execution->kill();
        }
    }
    auto cleanup_deadline = Clock::now() + args.limits.startup_time;
    auto wait_for_result = [&](int fd) {
        while (not ready(fd)) {
            auto remaining =
                std::chrono::ceil<std::chrono::milliseconds>(cleanup_deadline - Clock::now());
            if (remaining <= 0ms) {
                THROW("Communication cleanup timed out");
            }
            pollfd pfd{.fd = fd, .events = POLLIN, .revents = 0};
            if (poll(&pfd, 1, static_cast<int>(std::min<int64_t>(remaining.count(), 1000))) < 0 &&
                errno != EINTR)
            {
                THROW("poll()", errmsg());
            }
        }
    };
    if (not manager_result) {
        wait_for_result(manager.pollable_fd());
        manager_result = manager.await_result();
    }
    auto report = TestReport{
        .status = Status::CheckerError,
        .comment = {},
        .score = 0,
        .program = {},
        .checker =
            TestReport::Checker{
                .runtime = manager_result->runtime,
                .cpu_time = manager_result->cgroup.cpu_time.total(),
                .peak_memory_in_bytes = manager_result->cgroup.peak_memory_in_bytes,
            },
        .communication = TestReport::Communication{},
    };
    for (uint32_t i = 0; i < instances.size(); ++i) {
        auto& instance = instances[i];
        if (instance.execution && not instance.result) {
            wait_for_result(instance.execution->pollable_fd());
            instance.result = instance.execution->await_result();
            instance.execution.reset();
        }
        if (not instance.result) {
            continue;
        }
        auto& result = *instance.result;
        report.program.runtime = std::max(report.program.runtime, result.runtime);
        report.program.cpu_time = std::max(report.program.cpu_time, result.cgroup.cpu_time.total());
        report.program.peak_memory_in_bytes =
            std::max(report.program.peak_memory_in_bytes, result.cgroup.peak_memory_in_bytes);
        report.communication->total_cpu_time += result.cgroup.cpu_time.total();
        report.communication->instances.push_back({
            .id = i + 1,
            .status = instance.status,
            .program =
                {
                    .runtime = result.runtime,
                    .cpu_time = result.cgroup.cpu_time.total(),
                    .peak_memory_in_bytes = result.cgroup.peak_memory_in_bytes,
                },
            .output_size = instance.output_size,
            .cancelled = instance.cancelled,
        });
    }
    report.communication->elapsed = Clock::now() - started;
    std::optional<CheckerOutputReport> result;
    if (not manager_cancelled && not manager_failure) {
        result =
            get_checker_output_report(std::move(manager_report_fd), args.manager.max_comment_len);
        if (result->status == CheckerOutputReport::Status::CheckerError ||
            not std::isfinite(result->score) ||
            (finish && (*finish != (result->status == CheckerOutputReport::Status::OK))))
        {
            fail_manager("invalid or conflicting result");
        }
    }
    if (manager_failure) {
        report.status = manager_failure->status;
        report.comment = std::move(manager_failure->comment);
        return report;
    }
    if (program_failure) {
        report.status = program_failure->status;
        report.comment = std::move(program_failure->comment);
        return report;
    }
    if (manager_cancelled || not finish) {
        report.comment = "Manager error: missing result";
        return report;
    }
    report.status = *finish ? Status::OK : Status::WrongAnswer;
    report.score = result->score;
    report.comment = std::move(result->comment);
    report.communication->complete = *finish;
    return report;
}

} // namespace sim::judge

#include <fcntl.h>
#include <simlib/errmsg.hh>
#include <simlib/file_manip.hh>
#include <simlib/macros/throw.hh>
#include <simlib/overloaded.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/sim/judge/language_suite/suite.hh>
#include <simlib/temporary_file.hh>
#include <sys/eventfd.h>

namespace sim::judge::language_suite {

sandbox::result::Ok Suite::await_result(RunHandle&& run_handle) {
    return std::visit(
        overloaded{
            [](sandbox::result::Ok res_ok) { return res_ok; },
            [](sandbox::result::Error res_err) -> sandbox::result::Ok {
                THROW("Suite run error: ", res_err.description);
            }
        },
        sc.await_result(std::move(run_handle.request_handle))
    );
}

struct Suite::PreparedProgram::Impl {
    TemporaryFile artifact{"/tmp/sim_prepared_program.XXXXXX", S_IRUSR | S_IWUSR | S_IXUSR};
    FileDescriptor seccomp_bpf_fd;
    std::string interpreter;
    Runner runner;

    Impl(FilePath source, int bpf_fd, std::string interpreter, Runner runner)
    : seccomp_bpf_fd{fcntl(bpf_fd, F_DUPFD_CLOEXEC, 0)}
    , interpreter{std::move(interpreter)}
    , runner{runner} {
        if (!seccomp_bpf_fd.is_open()) {
            THROW("fcntl()", errmsg());
        }
        thread_fork_safe_copy(source, artifact.path(), S_IRUSR | S_IWUSR | S_IXUSR);
    }
};

Suite::PreparedProgram::PreparedProgram(std::shared_ptr<const Impl> impl) : impl{std::move(impl)} {}

Suite::PreparedProgram Suite::prepare_program(
    FilePath artifact, int seccomp_bpf_fd, std::string interpreter, Runner runner
) {
    return PreparedProgram{std::make_shared<PreparedProgram::Impl>(
        artifact, seccomp_bpf_fd, std::move(interpreter), runner
    )};
}

struct Suite::Execution::Impl {
    std::shared_ptr<const PreparedProgram::Impl> program;
    std::unique_ptr<sandbox::SupervisorConnection> sc =
        std::make_unique<sandbox::SupervisorConnection>(sandbox::spawn_supervisor({
            .cleanup_cgroup = true,
            .nonblocking_requests = true,
            .reset_signal_mask = true,
        }));
    FileDescriptor started_fd{eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)};
    RunHandle run_handle;
    sandbox::SupervisorConnection::KillRequestHandle kill_handle;
    bool awaited = false;

    Impl(
        std::shared_ptr<const PreparedProgram::Impl> program,
        Slice<std::string_view> args,
        RunOptions options,
        Slice<sandbox::RequestOptions::LinuxNamespaces::Mount::Operation> mount_ops
    )
    : program{std::move(program)}
    , run_handle{[&] {
        if (!started_fd.is_open()) {
            THROW("eventfd()", errmsg());
        }
        options.started_fd = started_fd;
        return this->program->runner(
            *sc,
            this->program->artifact.path(),
            this->program->seccomp_bpf_fd,
            this->program->interpreter,
            args,
            std::move(options),
            mount_ops
        );
    }()}
    , kill_handle{run_handle.request_handle.get_kill_handle()} {}

    ~Impl() noexcept(false) { delete sc.release(); }
};

Suite::Execution Suite::PreparedProgram::start_fresh(
    Slice<std::string_view> args,
    RunOptions options,
    Slice<sandbox::RequestOptions::LinuxNamespaces::Mount::Operation> mount_ops
) const {
    return Execution{std::make_unique<Execution::Impl>(impl, args, std::move(options), mount_ops)};
}

Suite::Execution::Execution(std::unique_ptr<Impl> impl) : impl{std::move(impl)} {}

Suite::Execution::Execution(Execution&&) noexcept = default;

Suite::Execution::~Execution() noexcept {
    try {
        delete impl.release();
    } catch (...) {
    }
}

int Suite::Execution::pollable_fd() const noexcept {
    return impl->awaited ? -1 : impl->run_handle.request_handle.pollable_fd();
}

int Suite::Execution::started_fd() const noexcept { return impl->started_fd; }

void Suite::Execution::kill() { impl->kill_handle.kill(); }

void Suite::Execution::cancel() {
    impl->run_handle.request_handle.cancel();
    impl->awaited = true;
}

sandbox::result::Ok Suite::Execution::await_result() {
    if (std::exchange(impl->awaited, true)) {
        THROW("cannot await an execution more than once");
    }
    auto result = impl->sc->await_result(std::move(impl->run_handle.request_handle));
    delete impl->sc.release();
    return std::visit(
        overloaded{
            [](sandbox::result::Ok res_ok) { return res_ok; },
            [](sandbox::result::Error res_err) -> sandbox::result::Ok {
                THROW("Suite run error: ", res_err.description);
            }
        },
        std::move(result)
    );
}

} // namespace sim::judge::language_suite

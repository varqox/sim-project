#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <simlib/file_descriptor.hh>
#include <simlib/result.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/sim/judge/compilation_cache.hh>
#include <simlib/slice.hh>
#include <string_view>
#include <vector>

namespace sim::judge::language_suite {

// A language suite interface
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class Suite {
protected:
    sandbox::SupervisorConnection sc = sandbox::spawn_supervisor();

public:
    Suite() = default;

    Suite(const Suite&) = delete;
    Suite(Suite&&) noexcept = default;
    Suite& operator=(const Suite&) = delete;
    Suite& operator=(Suite&&) = delete;

    virtual ~Suite() noexcept(false) = default;

    struct CompileOptions {
        std::chrono::nanoseconds time_limit;
        std::chrono::nanoseconds cpu_time_limit;
        uint64_t memory_limit_in_bytes;
        uint64_t max_file_size_in_bytes;

        struct Cache {
            CompilationCache& compilation_cache; // NOLINT
            std::string_view cached_name;
        };

        std::optional<Cache> cache = std::nullopt;
    };

    [[nodiscard]] virtual bool is_supported() = 0;

    virtual Result<std::optional<sandbox::result::Ok>, FileDescriptor>
    compile(FilePath source, CompileOptions options) = 0;

    struct RunOptions {
        std::optional<int> stdin_fd;
        std::optional<int> stdout_fd;
        std::optional<int> stderr_fd;
        std::chrono::nanoseconds time_limit;
        std::chrono::nanoseconds cpu_time_limit;
        uint64_t memory_limit_in_bytes;
        uint64_t max_stack_size_in_bytes;
        uint64_t max_file_size_in_bytes;
        uint32_t process_num_limit = 1;

        struct Rootfs {
            uint64_t max_total_size_of_files_in_bytes = 0;
            uint64_t inode_limit = 0;
        } rootfs = {};

        std::vector<std::string_view> env = {};
        std::optional<int> started_fd = std::nullopt;
    };

    struct [[nodiscard]] RunHandle {
        sandbox::SupervisorConnection::RequestHandle request_handle;

        explicit RunHandle(sandbox::SupervisorConnection::RequestHandle request_handle_) noexcept
        : request_handle{std::move(request_handle_)} {}
    };

    virtual RunHandle async_run(
        Slice<std::string_view> args,
        RunOptions options,
        Slice<sandbox::RequestOptions::LinuxNamespaces::Mount::Operation> mount_ops
    ) = 0;

    virtual sandbox::result::Ok await_result(RunHandle&& run_handle);

    class PreparedProgram;

    class Execution {
        struct Impl;
        std::unique_ptr<Impl> impl;

        explicit Execution(std::unique_ptr<Impl> impl);
        friend class PreparedProgram;

    public:
        Execution(const Execution&) = delete;
        Execution(Execution&&) noexcept;
        Execution& operator=(const Execution&) = delete;
        Execution& operator=(Execution&&) = delete;
        ~Execution() noexcept;

        [[nodiscard]] int pollable_fd() const noexcept;
        [[nodiscard]] int started_fd() const noexcept;
        void kill();
        void cancel();
        sandbox::result::Ok await_result();
    };

    class PreparedProgram {
        struct Impl;
        std::shared_ptr<const Impl> impl;

        explicit PreparedProgram(std::shared_ptr<const Impl> impl);
        friend class Suite;
        friend class Execution;

    public:
        [[nodiscard]] Execution start_fresh(
            Slice<std::string_view> args,
            RunOptions options,
            Slice<sandbox::RequestOptions::LinuxNamespaces::Mount::Operation> mount_ops = {}
        ) const;
    };

    [[nodiscard]] virtual PreparedProgram prepared_program() = 0;

protected:
    using Runner = RunHandle (*)(
        sandbox::SupervisorConnection&,
        FilePath,
        int,
        std::string_view,
        Slice<std::string_view>,
        RunOptions,
        Slice<sandbox::RequestOptions::LinuxNamespaces::Mount::Operation>
    );

    static PreparedProgram
    prepare_program(FilePath artifact, int seccomp_bpf_fd, std::string interpreter, Runner runner);
};

} // namespace sim::judge::language_suite

#include "get_checker_output_report.hh"

#include <fcntl.h>
#include <optional>
#include <simlib/errmsg.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/macros/throw.hh>
#include <simlib/pipe.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/sim/judge/test_on_test.hh>
#include <simlib/sim/judge/test_report.hh>
#include <simlib/splice_pipelines.hh>
#include <simlib/temporary_file.hh>
#include <simlib/working_directory.hh>
#include <string_view>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

using std::string_view;

using BindMount = sandbox::RequestOptions::LinuxNamespaces::Mount::BindMount;
using CreateFile = sandbox::RequestOptions::LinuxNamespaces::Mount::CreateFile;

namespace sim::judge {

TestReport test_on_test(TestArgs args) {
    static const auto page_size = sysconf(_SC_PAGESIZE);

    auto prog_output_file = TemporaryFile{"/tmp/sim-judge-test-on-test-program-output.XXXXXX"};
    auto prog_stdin_file_fd =
        FileDescriptor{args.test_input, O_RDONLY | O_LARGEFILE | O_CLOEXEC | O_NONBLOCK};
    if (!prog_stdin_file_fd.is_open()) {
        THROW("open(", args.test_input, ")", errmsg());
    }
    auto prog_stdout_file_fd =
        FileDescriptor{prog_output_file.path(), O_WRONLY | O_LARGEFILE | O_CLOEXEC | O_NONBLOCK};
    if (!prog_stdout_file_fd.is_open()) {
        THROW("open(", prog_output_file.path(), ")", errmsg());
    }

    auto stdin_pipe = pipe2(O_CLOEXEC);
    if (not stdin_pipe) {
        THROW("pipe2()", errmsg());
    }
    if (fcntl(stdin_pipe->writable, F_SETFL, O_NONBLOCK)) {
        THROW("fcntl()", errmsg());
    }

    auto stdout_pipe = pipe2(O_CLOEXEC);
    if (not stdout_pipe) {
        THROW("pipe2()", errmsg());
    }
    if (fcntl(stdout_pipe->readable, F_SETFL, O_NONBLOCK)) {
        THROW("fcntl()", errmsg());
    }

    auto prog_rh = args.compiled_program.async_run(
        {},
        {
            .stdin_fd = stdin_pipe->readable,
            .stdout_fd = stdout_pipe->writable,
            .stderr_fd = std::nullopt,
            .time_limit = args.program.time_limit,
            .cpu_time_limit = args.program.cpu_time_limit,
            // + page_size to allow detecting overuse
            .memory_limit_in_bytes = args.program.memory_limit_in_bytes + page_size,
            .max_stack_size_in_bytes = args.program.memory_limit_in_bytes + page_size,
            .max_file_size_in_bytes = 0,
            .process_num_limit = 1,
            .env = {},
        },
        {}
    );

    if (stdin_pipe->readable.close()) {
        THROW("close()", errmsg());
    }
    if (stdout_pipe->writable.close()) {
        THROW("close()", errmsg());
    }

    auto splice_res = splice_pipelines({
        Pipeline{
            .readable_fd = std::move(prog_stdin_file_fd),
            .writable_fd = std::move(stdin_pipe->writable),
        },
        Pipeline{
            .readable_fd = std::move(stdout_pipe->readable),
            .writable_fd = std::move(prog_stdout_file_fd),
            .transferred_data_limit_in_bytes = args.program.output_size_limit_in_bytes,
        },
    });

    auto prog_res = args.compiled_program.await_result(std::move(prog_rh));
    auto prog_cpu_time = prog_res.cgroup.cpu_time.total();
    auto report = TestReport{
        .status = TestReport::Status::OK,
        .comment = {},
        .score = 0,
        .program =
            {
                .runtime = prog_res.runtime,
                .cpu_time = prog_cpu_time,
                .peak_memory_in_bytes = prog_res.cgroup.peak_memory_in_bytes,
            },
        .checker = std::nullopt,
    };

    if (prog_res.runtime > args.program.time_limit || prog_cpu_time > args.program.cpu_time_limit) {
        report.status = TestReport::Status::TimeLimitExceeded;
        report.comment = "";
        return report;
    }

    if (prog_res.cgroup.peak_memory_in_bytes > args.program.memory_limit_in_bytes) {
        report.status = TestReport::Status::MemoryLimitExceeded;
        report.comment = "";
        return report;
    }

    if (splice_res.transferred_data_limit_exceeded[1]) {
        report.status = TestReport::Status::OutputSizeLimitExceeded;
        report.comment = "";
        return report;
    }

    if (prog_res.si != sandbox::Si{.code = CLD_EXITED, .status = 0}) {
        report.status = TestReport::Status::RuntimeError;
        report.comment = "Runtime error: " + prog_res.si.description();
        return report;
    }

    auto checker_output_fd = FileDescriptor{memfd_create("checker output", MFD_CLOEXEC)};
    if (!checker_output_fd.is_open()) {
        THROW("memfd_create()", errmsg());
    }
    auto checker_rh = args.compiled_checker.async_run(
        {{
            "/in",
            "/out",
            "/prog_out",
        }},
        {
            .stdin_fd = std::nullopt,
            .stdout_fd = checker_output_fd,
            .stderr_fd = checker_output_fd,
            .time_limit = args.checker.time_limit,
            .cpu_time_limit = args.checker.cpu_time_limit,
            // + page_size to allow detecting overuse
            .memory_limit_in_bytes = args.checker.memory_limit_in_bytes + page_size,
            .max_stack_size_in_bytes = args.checker.memory_limit_in_bytes + page_size,
            .max_file_size_in_bytes = args.checker.max_comment_len + 32,
            .process_num_limit = 1,
            .rootfs =
                {
                    .inode_limit = 3,
                },
            .env = {},
        },
        {{
            CreateFile{.path = "/../in"},
            CreateFile{.path = "/../out"},
            CreateFile{.path = "/../prog_out"},
            BindMount{
                .source = has_prefix(StringView{args.test_input}, "/")
                    ? string_view{args.test_input}
                    : string_view{concat_tostr(get_cwd(), args.test_input)},
                .dest = "/../in",
            },
            BindMount{
                .source = has_prefix(StringView{args.expected_output}, "/")
                    ? string_view{args.expected_output}
                    : string_view{concat_tostr(get_cwd(), args.expected_output)},
                .dest = "/../out",
            },
            BindMount{
                .source = string_view{prog_output_file.path()},
                .dest = "/../prog_out",
            },
        }}
    );
    auto checker_res = args.compiled_checker.await_result(std::move(checker_rh));
    report.checker = TestReport::Checker{
        .runtime = checker_res.runtime,
        .cpu_time = checker_res.cgroup.cpu_time.total(),
        .peak_memory_in_bytes = checker_res.cgroup.peak_memory_in_bytes,
    };

    if (checker_res.runtime > args.checker.time_limit ||
        report.checker->cpu_time > args.checker.cpu_time_limit)
    {
        report.status = TestReport::Status::CheckerError;
        report.comment = "Checker error: time limit exceeded";
        return report;
    }

    if (checker_res.cgroup.peak_memory_in_bytes > args.checker.memory_limit_in_bytes) {
        report.status = TestReport::Status::CheckerError,
        report.comment = "Checker error: memory limit exceeded";
        return report;
    }

    if (checker_res.si != sandbox::Si{.code = CLD_EXITED, .status = 0}) {
        report.status = TestReport::Status::CheckerError;
        report.comment = "Checker runtime error: " + checker_res.si.description();
        return report;
    }

    auto checker_output_report =
        get_checker_output_report(std::move(checker_output_fd), args.checker.max_comment_len);
    report.status = [&] {
        switch (checker_output_report.status) {
        case CheckerOutputReport::Status::CheckerError: return TestReport::Status::CheckerError;
        case CheckerOutputReport::Status::WrongAnswer: return TestReport::Status::WrongAnswer;
        case CheckerOutputReport::Status::OK: return TestReport::Status::OK;
        }
        __builtin_unreachable();
    }();
    report.comment = std::move(checker_output_report.comment);
    report.score = checker_output_report.score;
    return report;
}

} // namespace sim::judge

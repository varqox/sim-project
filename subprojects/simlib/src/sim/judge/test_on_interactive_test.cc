#include "get_checker_output_report.hh"

#include <fcntl.h>
#include <optional>
#include <simlib/errmsg.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/macros/throw.hh>
#include <simlib/pipe.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/sim/judge/test_on_interactive_test.hh>
#include <simlib/sim/judge/test_report.hh>
#include <simlib/splice_pipelines.hh>
#include <simlib/working_directory.hh>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

using BindMount = sandbox::RequestOptions::LinuxNamespaces::Mount::BindMount;
using CreateFile = sandbox::RequestOptions::LinuxNamespaces::Mount::CreateFile;

namespace sim::judge {

TestReport test_on_interactive_test(InteractiveTestArgs args) {
    static const auto page_size = sysconf(_SC_PAGESIZE);

    // Construction:
    // prog stdout ------> we ------> checker stdin
    // checker stdout ------> we ------> prog stdin

    auto prog_stdin_pipe = pipe2(O_CLOEXEC);
    if (not prog_stdin_pipe) {
        THROW("pipe2()", errmsg());
    }

    auto prog_stdout_pipe = pipe2(O_CLOEXEC);
    if (not prog_stdout_pipe) {
        THROW("pipe2()", errmsg());
    }

    auto checker_stdin_pipe = pipe2(O_CLOEXEC);
    if (not checker_stdin_pipe) {
        THROW("pipe2()", errmsg());
    }

    auto checker_stdout_pipe = pipe2(O_CLOEXEC);
    if (not checker_stdout_pipe) {
        THROW("pipe2()", errmsg());
    }

    auto checker_output_fd = FileDescriptor{memfd_create("checker output", MFD_CLOEXEC)};
    if (!checker_output_fd.is_open()) {
        THROW("memfd_create()", errmsg());
    }

    // Spawn the checker first to make the solution wait less
    auto checker_rh = args.compiled_checker.async_run(
        {{"/in"}},
        {
            .stdin_fd = checker_stdin_pipe->readable,
            .stdout_fd = checker_stdout_pipe->writable,
            .stderr_fd = checker_output_fd,
            .time_limit = args.checker.time_limit,
            .cpu_time_limit = args.checker.cpu_time_limit,
            // + page_size to allow detecting overuse
            .memory_limit_in_bytes = args.checker.memory_limit_in_bytes + page_size,
            .max_stack_size_in_bytes = args.checker.memory_limit_in_bytes + page_size,
            .max_file_size_in_bytes = args.checker.max_comment_len + 32, // + 32 for first two lines
            .process_num_limit = 1,
            .rootfs =
                {
                    .inode_limit = 1,
                },
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

    auto prog_rh = args.compiled_program.async_run(
        {},
        {
            .stdin_fd = prog_stdin_pipe->readable,
            .stdout_fd = prog_stdout_pipe->writable,
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

    if (prog_stdin_pipe->readable.close()) {
        THROW("close()", errmsg());
    }
    if (prog_stdout_pipe->writable.close()) {
        THROW("close()", errmsg());
    };
    if (checker_stdin_pipe->readable.close()) {
        THROW("close()", errmsg());
    }
    if (checker_stdout_pipe->writable.close()) {
        THROW("close()", errmsg());
    };

    auto splice_res = splice_pipelines({
        Pipeline{
            .readable_fd = std::move(prog_stdout_pipe->readable),
            .writable_fd = std::move(checker_stdin_pipe->writable),
        },
        Pipeline{
            .readable_fd = std::move(checker_stdout_pipe->readable),
            .writable_fd = std::move(prog_stdin_pipe->writable),
        },
    });

    // true iff program closed any pipe before checker closed any pipe
    auto program_res_is_more_important = !splice_res.first_broken_pipelines[0].writable &&
        !splice_res.first_broken_pipelines[1].readable;

    auto checker_res = args.compiled_checker.await_result(std::move(checker_rh));
    auto checker_cpu_time = checker_res.cgroup.cpu_time.total();
    auto checker_test_report = TestReport::Checker{
        .runtime = checker_res.runtime,
        .cpu_time = checker_cpu_time,
        .peak_memory_in_bytes = checker_res.cgroup.peak_memory_in_bytes,
    };

    auto kill_prog_and_contruct_report =
        [&](TestReport::Status status, std::string comment, double score) {
            prog_rh.request_handle.get_kill_handle().kill();
            auto prog_res = args.compiled_program.await_result(std::move(prog_rh));
            return TestReport{
                .status = status,
                .comment = std::move(comment),
                .score = score,
                .program =
                    {
                        .runtime = prog_res.runtime,
                        .cpu_time = prog_res.cgroup.cpu_time.total(),
                        .peak_memory_in_bytes = prog_res.cgroup.peak_memory_in_bytes,
                    },
                .checker = checker_test_report,
            };
        };

    if (checker_res.runtime > args.checker.time_limit ||
        checker_cpu_time > args.checker.cpu_time_limit)
    {
        return kill_prog_and_contruct_report(
            TestReport::Status::CheckerError, "Checker error: time limit exceeded", 0
        );
    }

    if (checker_res.cgroup.peak_memory_in_bytes > args.checker.memory_limit_in_bytes) {
        return kill_prog_and_contruct_report(
            TestReport::Status::CheckerError, "Checker error: memory limit exceeded", 0
        );
    }

    if (checker_res.si != sandbox::Si{.code = CLD_EXITED, .status = 0}) {
        return kill_prog_and_contruct_report(
            TestReport::Status::CheckerError,
            "Checker runtime error: " + checker_res.si.description(),
            0
        );
    }

    auto checker_output_report =
        get_checker_output_report(std::move(checker_output_fd), args.checker.max_comment_len);

    switch (checker_output_report.status) {
    case CheckerOutputReport::Status::CheckerError:
        return kill_prog_and_contruct_report(
            TestReport::Status::CheckerError, std::move(checker_output_report.comment), 0
        );
    case CheckerOutputReport::Status::WrongAnswer: {
        if (!program_res_is_more_important) {
            return kill_prog_and_contruct_report(
                TestReport::Status::WrongAnswer, std::move(checker_output_report.comment), 0
            );
        }
    } break;
    case CheckerOutputReport::Status::OK: {
        if (!program_res_is_more_important) {
            return kill_prog_and_contruct_report(
                TestReport::Status::OK,
                std::move(checker_output_report.comment),
                checker_output_report.score
            );
        }
    } break;
    }

    auto prog_res = args.compiled_program.await_result(std::move(prog_rh));
    auto prog_cpu_time = prog_res.cgroup.cpu_time.total();

    auto report = TestReport{
        .status = checker_output_report.status == CheckerOutputReport::Status::OK
            ? TestReport::Status::OK
            : TestReport::Status::WrongAnswer,
        .comment = std::move(checker_output_report.comment),
        .score = checker_output_report.score,
        .program =
            {
                .runtime = prog_res.runtime,
                .cpu_time = prog_cpu_time,
                .peak_memory_in_bytes = prog_res.cgroup.peak_memory_in_bytes,
            },
        .checker = checker_test_report,
    };

    if (report.status == TestReport::Status::OK || program_res_is_more_important) {
        if (prog_res.runtime > args.program.time_limit ||
            prog_cpu_time > args.program.cpu_time_limit)
        {
            report.status = TestReport::Status::TimeLimitExceeded;
            report.comment = "";
            report.score = 0;
            return report;
        }

        if (prog_res.cgroup.peak_memory_in_bytes > args.program.memory_limit_in_bytes) {
            report.status = TestReport::Status::MemoryLimitExceeded, report.comment = "";
            report.score = 0;
            return report;
        }

        if (prog_res.si != sandbox::Si{.code = CLD_EXITED, .status = 0}) {
            report.status = TestReport::Status::RuntimeError;
            report.comment = "Runtime error: " + prog_res.si.description();
            report.score = 0;
            return report;
        }
    }

    return report;
}

} // namespace sim::judge

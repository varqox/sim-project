#include "compile.hh"

#include <chrono>
#include <gtest/gtest.h>
#include <optional>
#include <simlib/file_contents.hh>
#include <simlib/sim/judge/language_suite/c_gcc.hh>
#include <simlib/sim/judge/language_suite/python.hh>
#include <simlib/sim/judge/test_on_communication_test.hh>
#include <simlib/temporary_file.hh>
#include <utility>

using sim::judge::TestReport;
using Status = TestReport::Status;
using namespace std::chrono_literals;

constexpr auto manager_helper = R"(
import os, struct, sys

def send(kind, ident=0, data=b''):
    sys.stdout.buffer.write(struct.pack('<III', kind, ident, len(data)) + data)
    sys.stdout.buffer.flush()

def receive():
    header = sys.stdin.buffer.read(12)
    if len(header) != 12: raise RuntimeError('missing event')
    kind, ident, size = struct.unpack('<III', header)
    data = sys.stdin.buffer.read(size)
    if len(data) != size: raise RuntimeError('truncated event')
    return kind, ident, data

send(1, 0, struct.pack('<I', 1))
assert receive()[0] == 101
next_id = 0

def batch(data):
    global next_id
    next_id += 1
    send(2, next_id)
    for i in range(0, len(data), 65536): send(3, next_id, data[i:i+65536])
    send(4, next_id)
    output = bytearray()
    while True:
        kind, ident, data = receive()
        assert ident == next_id
        if kind == 103: output.extend(data)
        if kind == 105: return bytes(output)

def start():
    global next_id
    next_id += 1
    send(5, next_id)
    return next_id

def finish(ok=True, score=100, comment=''):
    sys.stderr.write(('OK' if ok else 'WRONG') + '\n' + str(score) + '\n' + comment)
    sys.stderr.flush()
    send(8, 0, bytes([int(ok)]))
    sys.exit(0)
)";

struct Options {
    uint32_t instances = 2;
    std::optional<uint32_t> min_instances = std::nullopt;
    uint32_t max_concurrent = 1;
    std::chrono::nanoseconds cpu_time = 500ms;
    std::chrono::nanoseconds wall_time = 2s;
    uint64_t output_size = 64 << 20;
    uint64_t memory = 64 << 20;
    std::chrono::nanoseconds manager_cpu_time = 2s;
    std::chrono::nanoseconds manager_wall_time = 2s;
    std::chrono::nanoseconds startup_time = 1s;
    uint64_t session_data = 256 << 20;
    bool handshake = true;
};

static TestReport judge(
    sim::judge::language_suite::Suite& program,
    StringView manager_source,
    const Options& options = {}
) {
    static sim::judge::language_suite::Python manager;
    auto source = concat_tostr(options.handshake ? manager_helper : "", manager_source);
    compile(manager, source);
    auto input = TemporaryFile{"/tmp/sim_communication_test_input.XXXXXX"};
    put_file_contents(input.path(), "private test input\n");
    return sim::judge::test_on_communication_test({
        .compiled_program = program,
        .compiled_manager = manager,
        .test_input = input.path(),
        .instances = options.instances,
        .min_instances = options.min_instances,
        .max_concurrent = options.max_concurrent,
        .program =
            {
                .time_limit = options.wall_time,
                .cpu_time_limit = options.cpu_time,
                .memory_limit_in_bytes = options.memory,
                .output_size_limit_in_bytes = options.output_size,
            },
        .manager =
            {
                .time_limit = options.manager_wall_time,
                .cpu_time_limit = options.manager_cpu_time,
                .memory_limit_in_bytes = 128 << 20,
                .max_comment_len = 256,
            },
        .limits = {
            .session_data_in_bytes = options.session_data,
            .startup_time = options.startup_time,
        },
    });
}

static TestReport
judge(StringView program_source, StringView manager_source, const Options& options = {}) {
    static sim::judge::language_suite::Python program;
    compile(program, program_source);
    return judge(program, manager_source, options);
}

constexpr auto echo = "import sys; sys.stdout.buffer.write(sys.stdin.buffer.read())";

// NOLINTNEXTLINE
TEST(test_on_communication_test, two_buffered_batch_runs) {
    auto report = judge(echo, R"(
data = b'hello\x00world\n'
assert batch(batch(data)) == data
finish()
)");
    EXPECT_EQ(report.status, Status::OK) << report.comment;
    EXPECT_EQ(report.score, 1);
    ASSERT_TRUE(report.communication);
    EXPECT_TRUE(report.communication->complete);
    ASSERT_EQ(report.communication->instances.size(), 2);
    EXPECT_EQ(report.communication->instances[0].output_size, 12);
    EXPECT_GE(report.communication->total_cpu_time, report.program.cpu_time);
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, compiled_batch_reads_to_eof) {
    sim::judge::language_suite::C_GCC program{sim::judge::language_suite::C_GCC::Standard::C11};
    if (not program.is_supported()) {
        GTEST_SKIP();
    }
    compile(
        program,
        "#include <stdio.h>\nint main(void) { int c; while ((c = getchar()) != EOF) putchar(c); }"
    );
    auto report =
        judge(program, "data = b'x' * 262144; assert batch(batch(data)) == data; finish()\n");
    EXPECT_EQ(report.status, Status::OK) << report.comment;
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, no_transfer_joint_checking) {
    auto report =
        judge(echo, "a = batch(b'first'); b = batch(b'second'); finish(a + b == b'firstsecond')\n");
    EXPECT_EQ(report.status, Status::OK) << report.comment;
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, contestant_output_cannot_issue_commands) {
    auto report = judge(echo, R"(
commands = struct.pack('<III', 1, 0, 4) + struct.pack('<I', 1)
commands += struct.pack('<III', 8, 0, 1) + b'\x01'
assert batch(commands) == commands
finish(False, 0, 'Only the manager decides')
)");
    EXPECT_EQ(report.status, Status::WrongAnswer) << report.comment;
    EXPECT_EQ(report.comment, "Only the manager decides");
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, three_runs_and_partial_score) {
    auto report = judge(
        echo,
        "assert batch(batch(batch(b'test'))) == b'test'; finish(True, 42.5, 'partial')\n",
        {.instances = 3}
    );
    EXPECT_EQ(report.status, Status::OK) << report.comment;
    EXPECT_DOUBLE_EQ(report.score, 0.425);
    EXPECT_EQ(report.comment, "partial");
    EXPECT_TRUE(report.communication->complete);
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, variable_instance_count) {
    for (auto count : {1, 2, 32}) {
        SCOPED_TRACE(count);
        auto source = concat_tostr(
            "for i in range(", count, "): assert batch(b'test') == b'test'\nfinish()\n"
        );
        auto report = judge(echo, source, {.instances = 32, .min_instances = 1});
        EXPECT_EQ(report.status, Status::OK) << report.comment;
        EXPECT_EQ(report.score, 1);
        ASSERT_TRUE(report.communication);
        EXPECT_TRUE(report.communication->complete);
        EXPECT_EQ(report.communication->instances.size(), count);
    }
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, required_instance_count) {
    for (auto options :
         {Options{}, Options{.min_instances = 2}, Options{.instances = 32, .min_instances = 2}})
    {
        auto report = judge(echo, "batch(b'test'); finish()\n", options);
        EXPECT_EQ(report.status, Status::CheckerError) << report.comment;
        EXPECT_NE(report.comment.find("before all instances"), std::string::npos);
    }
    auto report = judge(echo, "batch(b'1'); batch(b'2'); finish()\n", {.min_instances = 2});
    EXPECT_EQ(report.status, Status::OK) << report.comment;
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, variable_count_exceeding_maximum) {
    auto report =
        judge(echo, "batch(b'1'); batch(b'2'); batch(b'3'); finish()\n", {.min_instances = 1});
    EXPECT_EQ(report.status, Status::CheckerError) << report.comment;
    EXPECT_NE(report.comment.find("invalid instance reservation"), std::string::npos);
    ASSERT_TRUE(report.communication);
    EXPECT_EQ(report.communication->instances.size(), 2);
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, variable_count_unfinished_instances) {
    for (auto body :
         {"finish()\n", "batch(b'1'); send(2, 2); finish()\n", "batch(b'1'); start(); finish()\n"})
    {
        SCOPED_TRACE(body);
        auto report = judge(echo, body, {.min_instances = 1});
        EXPECT_EQ(report.status, Status::CheckerError) << report.comment;
        EXPECT_NE(report.comment.find("before all instances"), std::string::npos);
    }
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, batch_then_streaming) {
    auto report = judge(echo, R"(
data = batch(b'secret')
ident = start()
send(6, ident, data)
send(7, ident)
output = bytearray()
while True:
    kind, current, data = receive()
    assert current == ident
    if kind == 103: output.extend(data)
    if kind == 105: break
finish(output == b'secret')
)");
    EXPECT_EQ(report.status, Status::OK) << report.comment;
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, concurrent_participants) {
    auto report = judge(
        R"(
import sys
role = sys.stdin.readline().strip()
print(role, flush=True)
value = sys.stdin.readline().strip()
print(role + value, flush=True)
)",
        R"(
a, b = start(), start()
send(6, a, b'A\n')
send(6, b, b'B\n')
outputs = {a: bytearray(), b: bytearray()}
while any(b'\n' not in data for data in outputs.values()):
    kind, ident, data = receive()
    if kind == 103: outputs[ident].extend(data)
send(6, a, b'1\n'); send(7, a)
send(6, b, b'2\n'); send(7, b)
ended = 0
while ended != 2:
    kind, ident, data = receive()
    if kind == 103: outputs[ident].extend(data)
    if kind == 105: ended += 1
finish(outputs[a] == b'A\nA1\n' and outputs[b] == b'B\nB2\n')
)",
        {.max_concurrent = 2}
    );
    EXPECT_EQ(report.status, Status::OK) << report.comment;
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, fresh_ipc_namespace) {
    auto report = judge(
        R"(
import ctypes, sys
libc = ctypes.CDLL(None, use_errno=True)
key = 19088743
if sys.stdin.read() == 'create':
    ident = libc.semget(key, 1, 0o1000 | 0o2000 | 0o600)
    assert ident >= 0
    assert libc.semctl(ident, 0, 16, 31415) == 0
else:
    assert libc.semget(key, 1, 0o600) == -1
    assert ctypes.get_errno() == 2
)",
        "batch(b'create'); batch(b'check'); finish()\n"
    );
    EXPECT_EQ(report.status, Status::OK) << report.comment;
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, per_instance_cpu_limit) {
    auto report = judge(
        R"(
import time
end = time.process_time() + 0.15
while time.process_time() < end: pass
)",
        "batch(b''); batch(b''); finish()\n",
        {.cpu_time = 250ms}
    );
    EXPECT_EQ(report.status, Status::OK) << report.comment;
    EXPECT_GT(report.communication->total_cpu_time, 250ms);
    EXPECT_LT(report.program.cpu_time, 250ms);
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, second_instance_runtime_error) {
    auto report = judge(
        "import sys; sys.exit(int(sys.stdin.read()))", "batch(b'0'); batch(b'3'); finish()\n"
    );
    EXPECT_EQ(report.status, Status::RuntimeError) << report.comment;
    EXPECT_NE(report.comment.find("Instance 2"), std::string::npos);
    EXPECT_FALSE(report.communication->complete);
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, contestant_cpu_limit) {
    auto report = judge("while True: pass", "batch(b''); finish()\n", {.cpu_time = 50ms});
    EXPECT_EQ(report.status, Status::TimeLimitExceeded) << report.comment;
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, contestant_memory_limit) {
    auto report = judge("a = bytearray(256 << 20)", "batch(b''); finish()\n");
    EXPECT_EQ(report.status, Status::MemoryLimitExceeded) << report.comment;
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, contestant_output_limit) {
    auto report = judge("print('a' * 100000)", "batch(b''); finish()\n", {.output_size = 100});
    EXPECT_EQ(report.status, Status::OutputSizeLimitExceeded) << report.comment;
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, manager_cpu_limit) {
    auto report = judge(echo, "while True: pass\n", {.manager_cpu_time = 50ms});
    EXPECT_EQ(report.status, Status::CheckerError) << report.comment;
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, early_wrong_answer_cancels_participant) {
    auto report =
        judge("import time; time.sleep(100)", "start(); finish(False, 0, 'invalid message')\n");
    EXPECT_EQ(report.status, Status::WrongAnswer) << report.comment;
    EXPECT_EQ(report.comment, "invalid message");
    ASSERT_EQ(report.communication->instances.size(), 1);
    EXPECT_TRUE(report.communication->instances[0].cancelled);
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, premature_success) {
    auto report = judge(echo, "finish()\n");
    EXPECT_EQ(report.status, Status::CheckerError);
    EXPECT_NE(report.comment.find("before all instances"), std::string::npos);
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, invalid_protocol_and_lifecycle) {
    for (auto body :
         {
             "send(1, 0, b'bad'); finish()\n",
             "send(99); finish()\n",
             "send(6, 1, b'input'); finish()\n",
             "send(2, 1); send(2, 1); finish()\n",
             "send(2, 1); send(4, 1); send(4, 1); finish()\n",
             "start(); start(); finish()\n",
             "send(2, 1); send(2, 2); send(2, 3); finish()\n",
             "send(2, 1); send(7, 1); finish()\n",
             "sys.stdout.buffer.write(struct.pack('<III', 3, 1, 65537)); "
             "sys.stdout.buffer.flush()\n",
             "sys.stdout.buffer.write(b'xx'); sys.stdout.buffer.flush()\n",
             "sys.exit(0)\n",
         })
    {
        SCOPED_TRACE(body);
        auto report = judge(echo, body);
        EXPECT_EQ(report.status, Status::CheckerError) << report.comment;
    }
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, invalid_handshake) {
    auto report = judge(
        echo,
        "import sys; sys.stdout.buffer.write(b'not a frame'); sys.stdout.buffer.flush()\n",
        {.handshake = false}
    );
    EXPECT_EQ(report.status, Status::CheckerError) << report.comment;
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, stdout_eof_is_not_completion) {
    auto report = judge(
        "import os, time; os.close(1); time.sleep(100)",
        "batch(b''); finish()\n",
        {.wall_time = 100ms}
    );
    EXPECT_EQ(report.status, Status::TimeLimitExceeded) << report.comment;
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, private_input_only_in_manager) {
    auto report = judge(
        R"(
import os
assert not os.path.exists('/in')
assert not os.path.exists('/proc/self/fd')
)",
        "assert open('/in').read() == 'private test input\\n'; batch(b''); batch(b''); finish()\n"
    );
    EXPECT_EQ(report.status, Status::OK) << report.comment;
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, session_input_limit) {
    auto report = judge(echo, "batch(b'a' * 65536); finish()\n", {.session_data = 1024});
    EXPECT_EQ(report.status, Status::CheckerError) << report.comment;
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, conflicting_final_result) {
    auto report =
        judge(echo, "sys.stderr.write('OK\\n100\\n'); sys.stderr.flush(); send(8, 0, b'\\x00')\n");
    EXPECT_EQ(report.status, Status::CheckerError) << report.comment;
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, streaming_runs_with_private_manager_state) {
    auto report = judge(
        R"(
import sys
for line in sys.stdin:
    value = int(line)
    print(value * value, flush=True)
)",
        R"(
state = []
for value in (7, 11):
    ident = start()
    send(6, ident, str(value).encode() + b'\n')
    output = bytearray()
    while b'\n' not in output:
        kind, current, data = receive()
        assert current == ident
        if kind == 103: output.extend(data)
    state.append(int(output))
    send(7, ident)
    while receive()[0] != 105: pass
finish(state == [49, 121])
)"
    );
    EXPECT_EQ(report.status, Status::OK) << report.comment;
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, streaming_then_batch_with_channel_transform) {
    auto report = judge(echo, R"(
ident = start()
send(6, ident, b'cba\x00fed'); send(7, ident)
output = bytearray()
while True:
    kind, current, data = receive()
    assert current == ident
    if kind == 103: output.extend(data)
    if kind == 105: break
finish(batch(bytes(sorted(output))) == b'\x00abcdef')
)");
    EXPECT_EQ(report.status, Status::OK) << report.comment;
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, fragmented_commands) {
    auto report = judge(echo, R"(
def send(kind, ident=0, data=b''):
    frame = struct.pack('<III', kind, ident, len(data)) + data
    for byte in frame: os.write(1, bytes([byte]))
assert batch(batch(b'fragmented\x00\n')) == b'fragmented\x00\n'
finish()
)");
    EXPECT_EQ(report.status, Status::OK) << report.comment;
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, large_streaming_input_and_output) {
    auto report = judge(
        R"(
import os
while True:
    data = os.read(0, 4096)
    if not data: break
    os.write(1, data)
)",
        R"(
ident = start()
for i in range(64): send(6, ident, bytes([i]) * 65536)
send(7, ident)
output = bytearray()
while True:
    kind, current, data = receive()
    if kind == 103: output.extend(data)
    if kind == 105: break
finish(output == b''.join(bytes([i]) * 65536 for i in range(64)))
)",
        {.instances = 1}
    );
    EXPECT_EQ(report.status, Status::OK) << report.comment;
    EXPECT_EQ(report.communication->instances[0].output_size, 4 << 20);
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, started_output_closed_and_ended_order) {
    auto report = judge(
        echo,
        R"(
for ident in range(1, 33):
    send(2, ident); send(3, ident, b'x'); send(4, ident)
    events = []
    output = bytearray()
    while True:
        kind, current, data = receive()
        assert current == ident
        events.append(kind)
        if kind == 103: output.extend(data)
        if kind == 105: break
    assert events[0] == 102
    assert events[-2:] == [104, 105]
    assert events.count(102) == 1 and events.count(104) == 1
    assert output == b'x'
finish()
)",
        {.instances = 32}
    );
    EXPECT_EQ(report.status, Status::OK) << report.comment;
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, manager_runtime_and_memory_errors) {
    for (auto source : {"os.abort()\n", "a = bytearray(512 << 20)\n"}) {
        SCOPED_TRACE(source);
        auto report = judge(echo, source);
        EXPECT_EQ(report.status, Status::CheckerError) << report.comment;
    }
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, invalid_manager_results) {
    for (auto result : {"", "garbage", "OK\\n101\\n", "OK\\n-1\\n", "OK\\nnan\\n"}) {
        SCOPED_TRACE(result);
        auto source = concat_tostr(
            "batch(b''); sys.stderr.write('",
            result,
            "'); sys.stderr.flush(); send(8, 0, b'\\x01')\n"
        );
        auto report = judge(echo, source, {.instances = 1});
        EXPECT_EQ(report.status, Status::CheckerError) << report.comment;
    }
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, session_output_limit) {
    for (auto source : {"batch(b''); finish()\n", "start(); receive(); receive()\n"}) {
        SCOPED_TRACE(source);
        auto report = judge("print('a' * 65536)", source, {.session_data = 16384});
        EXPECT_EQ(report.status, Status::OutputSizeLimitExceeded) << report.comment;
    }
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, stalled_session) {
    auto report = judge(
        "import sys; sys.stdin.read()",
        "start(); receive(); receive()\n",
        {
            .instances = 1,
            .wall_time = 50ms,
            .manager_wall_time = 50ms,
            .startup_time = 500ms,
        }
    );
    EXPECT_EQ(report.status, Status::TimeLimitExceeded) << report.comment;
    EXPECT_NE(report.comment.find("session"), std::string::npos);
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, stalled_manager_without_participants) {
    auto report = judge(
        echo,
        "import time; time.sleep(100)\n",
        {
            .instances = 1,
            .wall_time = 50ms,
            .manager_wall_time = 50ms,
            .startup_time = 500ms,
        }
    );
    EXPECT_EQ(report.status, Status::CheckerError) << report.comment;
    EXPECT_NE(report.comment.find("session"), std::string::npos);
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, input_receiver_exits_early) {
    auto report = judge(
        "import os, time; os.close(0); time.sleep(0.2)",
        R"(
ident = start()
for i in range(32): send(6, ident, b'x' * 65536)
send(7, ident)
while receive()[0] != 105: pass
finish()
)",
        {.instances = 1}
    );
    EXPECT_EQ(report.status, Status::OK) << report.comment;
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, input_receiver_exits_before_queued_input_is_read) {
    auto report = judge(
        "import os; os._exit(0)",
        R"(
ident = start()
for i in range(1000): send(6, ident, b'x' * 65536)
while receive()[0] != 105: pass
send(6, ident, b'last input')
send(7, ident)
finish(False, 0, 'Participant stopped reading')
)",
        {.instances = 1}
    );
    EXPECT_EQ(report.status, Status::WrongAnswer) << report.comment;
    EXPECT_EQ(report.comment, "Participant stopped reading");
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, invalid_limits) {
    for (auto options :
         {
             Options{.instances = 0},
             Options{.instances = 33},
             Options{.min_instances = 0},
             Options{.min_instances = 3},
             Options{.max_concurrent = 0},
             Options{.max_concurrent = 3},
             Options{.cpu_time = -1ns},
             Options{.wall_time = -1ns},
             Options{.wall_time = 25h},
             Options{.memory = 0},
             Options{.memory = 2ULL << 30},
             Options{.startup_time = 0ns},
         })
    {
        EXPECT_THROW(judge(echo, "finish()\n", options), std::exception);
    }
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, maximum_concurrent_participants) {
    auto report = judge(
        echo,
        R"(
for ident in range(1, 9):
    assert start() == ident
    send(6, ident, bytes([ident]) * 65536)
    send(7, ident)
outputs = {ident: bytearray() for ident in range(1, 9)}
ended = set()
while len(ended) != 8:
    kind, ident, data = receive()
    if kind == 103: outputs[ident].extend(data)
    if kind == 105: ended.add(ident)
finish(all(data == bytes([ident]) * 65536 for ident, data in outputs.items()))
)",
        {.instances = 8, .max_concurrent = 8}
    );
    EXPECT_EQ(report.status, Status::OK) << report.comment;
    ASSERT_EQ(report.communication->instances.size(), 8);
    for (const auto& instance : report.communication->instances) {
        EXPECT_EQ(instance.status, Status::OK);
        EXPECT_FALSE(instance.cancelled);
    }
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, protocol_error_cancels_every_participant) {
    auto report = judge(
        "import sys; sys.stdin.read()",
        "start(); start(); send(99); receive()\n",
        {.max_concurrent = 2}
    );
    EXPECT_EQ(report.status, Status::CheckerError) << report.comment;
    ASSERT_EQ(report.communication->instances.size(), 2);
    for (const auto& instance : report.communication->instances) {
        EXPECT_TRUE(instance.cancelled);
    }
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, success_does_not_hide_runtime_error) {
    auto report = judge(
        "import os; os.close(1); raise RuntimeError()",
        R"(
ident = start()
while receive()[0] != 104: pass
finish()
)",
        {.instances = 1}
    );
    EXPECT_NE(report.status, Status::OK);
    EXPECT_EQ(report.score, 0);
    EXPECT_FALSE(report.communication->complete);
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, manager_cannot_skip_final_exit) {
    auto report = judge(
        echo,
        R"(
batch(b'')
sys.stderr.write('OK\n100\n'); sys.stderr.flush()
send(8, 0, b'\x01')
os.abort()
)",
        {.instances = 1}
    );
    EXPECT_EQ(report.status, Status::CheckerError) << report.comment;
    EXPECT_FALSE(report.communication->complete);
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, commands_after_finish) {
    auto report = judge(
        echo,
        R"(
sys.stderr.write('WRONG\n0\n'); sys.stderr.flush()
send(8, 0, b'\x00'); send(2, 1)
)"
    );
    EXPECT_EQ(report.status, Status::CheckerError) << report.comment;
}

// NOLINTNEXTLINE
TEST(test_on_communication_test, variable_parallelism_with_live_barriers) {
    constexpr auto source = R"(
import sys
ident, value = map(int, input().split())
print('ready', flush=True)
command, increment = input().split()
assert command == 'add'
print(value + int(increment), flush=True)
assert input() == 'done'
)";
    constexpr auto manager = R"(
for first in range(1, total + 1, concurrent):
    ids = [start() for _ in range(concurrent)]
    for ident in ids: send(6, ident, f'{ident} {17 + ident}\n'.encode())
    def responses(ready):
        outputs = {ident: b'' for ident in ids}
        received = set()
        while len(received) != concurrent:
            kind, ident, data = receive()
            assert ident in ids and kind != 105
            if kind != 103: continue
            assert ident not in received
            outputs[ident] += data
            assert len(outputs[ident]) < 32
            if b'\n' in outputs[ident]:
                assert outputs[ident] == (b'ready\n' if ready else f'{17 + 2 * ident}\n'.encode())
                received.add(ident)
    responses(True)
    for ident in ids: send(6, ident, f'add {ident}\n'.encode())
    responses(False)
    for ident in ids:
        send(6, ident, b'done\n')
        send(7, ident)
    ended = set()
    while len(ended) != concurrent:
        kind, ident, data = receive()
        assert ident in ids and kind != 103
        if kind == 105: ended.add(ident)
finish()
)";
    for (auto counts : {std::pair{2, 1}, std::pair{4, 2}, std::pair{8, 8}, std::pair{32, 4}}) {
        SCOPED_TRACE(concat_tostr(counts.first, '/', counts.second));
        auto report = judge(
            source,
            from_unsafe{concat_tostr(
                "total, concurrent = ", counts.first, ", ", counts.second, '\n', manager
            )},
            {.instances = 32, .min_instances = 2, .max_concurrent = 8, .memory = 128 << 20}
        );
        EXPECT_EQ(report.status, Status::OK) << report.comment;
        EXPECT_EQ(report.score, 1);
        ASSERT_TRUE(report.communication);
        EXPECT_TRUE(report.communication->complete);
        ASSERT_EQ(report.communication->instances.size(), static_cast<size_t>(counts.first));
        for (const auto& instance : report.communication->instances) {
            EXPECT_EQ(instance.status, Status::OK);
            EXPECT_FALSE(instance.cancelled);
        }
    }
}

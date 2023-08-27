#pragma once

#include <simlib/sandbox/seccomp/bpf_builder.hh>

namespace sandbox::seccomp {

void allow_common_safe_syscalls(BpfBuilder& bpf);

} // namespace sandbox::seccomp

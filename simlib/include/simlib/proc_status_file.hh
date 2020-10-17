#pragma once

#include "simlib/file_descriptor.hh"
#include "simlib/inplace_buff.hh"
#include "simlib/string_view.hh"

// On error returns empty (not open) file descriptor and errno is set open()
FileDescriptor open_proc_status(pid_t tid) noexcept;

// Returns contents of the field @p field_name from @p proc_status_fd
// E.g. requesting "VmPeak" from file with line "VmPeak:  19096 kB" will return
// "19096 kB"
// On error throws std::runtime_exception
InplaceBuff<24> field_from_proc_status(int proc_status_fd,
                                       StringView field_name);

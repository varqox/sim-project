#pragma once

#include <simlib/string_view.hh>
#include <utime.h>

namespace sim::job_server {

constexpr CStringView notify_file = ".job-server.notify";

// Notifies the Job server that there are jobs to do
inline void notify_job_server() noexcept { (void)utime(notify_file.c_str(), nullptr); }

} // namespace sim::job_server

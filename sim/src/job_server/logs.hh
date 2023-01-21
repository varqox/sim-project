#pragma once

#include <simlib/string_view.hh>

namespace job_server {

static constexpr CStringView stdlog_file = "logs/job-server.log";
static constexpr CStringView errlog_file = "logs/job-server-error.log";

} // namespace job_server

#pragma once

#include "simlib/string_view.hh"

namespace web_server {

static constexpr CStringView stdlog_file = "logs/web-server.log";
static constexpr CStringView errlog_file = "logs/web-server-error.log";

} // namespace web_server

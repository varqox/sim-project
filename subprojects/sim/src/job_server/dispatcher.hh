#pragma once

#include <cstdint>
#include <optional>
#include <sim/jobs/old_job.hh>
#include <sim/mysql/mysql.hh>
#include <simlib/string_view.hh>

namespace job_server {

void job_dispatcher(
    sim::mysql::Connection& mysql,
    uint64_t job_id,
    sim::jobs::OldJob::Type jtype,
    std::optional<uint64_t> file_id,
    std::optional<uint64_t> aux_id,
    StringView info,
    StringView created_at
);

} // namespace job_server

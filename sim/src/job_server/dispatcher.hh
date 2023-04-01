#pragma once

#include <cstdint>
#include <optional>
#include <sim/jobs/job.hh>
#include <simlib/string_view.hh>

namespace job_server {

void job_dispatcher(
    uint64_t job_id,
    sim::jobs::Job::Type jtype,
    std::optional<uint64_t> file_id,
    std::optional<uint64_t> tmp_file_id,
    std::optional<StringView> creator,
    std::optional<uint64_t> aux_id,
    StringView info,
    StringView added
);

} // namespace job_server

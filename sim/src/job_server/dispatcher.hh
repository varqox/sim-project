#pragma once

#include "sim/constants.hh"

#include <optional>

namespace job_server {

void job_dispatcher(
    uint64_t job_id, sim::JobType jtype, std::optional<uint64_t> file_id,
    std::optional<uint64_t> tmp_file_id, std::optional<StringView> creator,
    std::optional<uint64_t> aux_id, StringView info, StringView added);

} // namespace job_server

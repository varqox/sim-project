#pragma once

#include <optional>
#include <sim/constants.hh>

void job_dispatcher(
    uint64_t job_id, JobType jtype, std::optional<uint64_t> file_id,
    std::optional<uint64_t> tmp_file_id, std::optional<StringView> creator,
    std::optional<uint64_t> aux_id, StringView info, StringView added);

#pragma once

#include <sim/constants.h>
#include <simlib/optional.h>

void job_dispatcher(uint64_t job_id, JobType jtype, Optional<uint64_t> file_id,
                    Optional<uint64_t> tmp_file_id,
                    Optional<StringView> creator, Optional<uint64_t> aux_id,
                    StringView info, StringView added);

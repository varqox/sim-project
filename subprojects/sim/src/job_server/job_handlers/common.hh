#pragma once

#include <sim/jobs/job.hh>
#include <sim/mysql/mysql.hh>
#include <simlib/concat_common.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/logger.hh>
#include <simlib/syscalls.hh>
#include <string>
#include <type_traits>
#include <utility>

namespace job_server::job_handlers {

class Logger {
    std::string str;

public:
    Logger() noexcept = default;
    Logger(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger& operator=(Logger&&) = delete;
    ~Logger() = default;

    template <class... Args, std::enable_if_t<(is_string_argument<Args> and ...), int> = 0>
    void operator()(Args&&... args) {
        back_insert(str, args..., '\n');
        stdlog('[', syscalls::gettid(), "] ", std::forward<Args>(args)...);
    }

    [[nodiscard]] const std::string& get_logs() const noexcept { return str; }
};

void mark_job_as_done(
    sim::mysql::Connection& mysql, const Logger& logger, decltype(sim::jobs::Job::id) job_id
);

void mark_job_as_cancelled(
    sim::mysql::Connection& mysql, const Logger& logger, decltype(sim::jobs::Job::id) job_id
);

void mark_job_as_failed(
    sim::mysql::Connection& mysql, const Logger& logger, decltype(sim::jobs::Job::id) job_id
);

void update_job_log(
    sim::mysql::Connection& mysql, const Logger& logger, decltype(sim::jobs::Job::id) job_id
);

} // namespace job_server::job_handlers

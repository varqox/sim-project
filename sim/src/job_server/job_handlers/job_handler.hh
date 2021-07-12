#pragma once

#include "simlib/sim/conver.hh"

namespace job_server::job_handlers {

class JobHandler {
private:
    bool job_failed_ = false;

protected:
    const uint64_t job_id_;
    InplaceBuff<1 << 14> job_log_holder_;

    explicit JobHandler(uint64_t job_id)
    : job_id_(job_id) {}

    template <class... Args>
    auto job_log(Args&&... args) {
        return DoubleAppender<decltype(job_log_holder_)>(
            stdlog, job_log_holder_, std::forward<Args>(args)...);
    }

    template <class... Args>
    void set_failure(Args&&... args) {
        job_log(std::forward<Args>(args)...);
        job_failed_ = true;
    }

    template <class Arg1, class... Args>
    void job_cancelled(Arg1&& arg1, Args&&... args) {
        job_log(std::forward<Arg1>(arg1), std::forward<Args>(args)...);
        job_canceled();
    }

    virtual void job_canceled();

    virtual void job_done();

    virtual void job_done(StringView new_info);

public:
    JobHandler(const JobHandler&) = delete;
    JobHandler(JobHandler&&) = delete;
    JobHandler& operator=(const JobHandler&) = delete;
    JobHandler& operator=(JobHandler&&) = delete;
    virtual ~JobHandler() = default;

    virtual void run() = 0;

    [[nodiscard]] bool failed() const noexcept { return job_failed_; }

    [[nodiscard]] const auto& get_log() const noexcept { return job_log_holder_; }
};

} // namespace job_server::job_handlers

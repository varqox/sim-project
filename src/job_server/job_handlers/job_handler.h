#pragma once

#include <simlib/sim/conver.h>

namespace job_handlers {

class JobHandler {
private:
	bool job_failed = false;

protected:
	const uint64_t job_id;
	InplaceBuff<1 << 14> job_log_holder;

	JobHandler(uint64_t job_id) : job_id(job_id) {}

	JobHandler(const JobHandler&) = delete;
	JobHandler(JobHandler&&) = delete;
	JobHandler& operator=(const JobHandler&) = delete;
	JobHandler& operator=(JobHandler&&) = delete;

	template <class... Args>
	auto job_log(Args&&... args) {
		return DoubleAppender<decltype(job_log_holder)>(
		   stdlog, job_log_holder, std::forward<Args>(args)...);
	}

	template <class... Args>
	void set_failure(Args&&... args) {
		job_log(std::forward<Args>(args)...);
		job_failed = true;
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
	virtual void run() = 0;

	bool failed() const noexcept { return job_failed; }

	const auto& get_log() const noexcept { return job_log_holder; }

	virtual ~JobHandler() = default;
};

} // namespace job_handlers

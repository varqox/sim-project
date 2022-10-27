#pragma once

#include <simlib/concurrent/bounded_queue.hh>
#include <thread>
#include <vector>

namespace concurrent {

template <class Job>
class JobProcessor {
    BoundedQueue<Job> jobs_;

    void worker() {
        for (;;) {
            auto job = jobs_.pop_opt();
            if (not job.has_value()) {
                return;
            }

            process_job(job.value());
        }
    }

    void spawn_workers(std::vector<std::thread>& workers) {
        for (std::thread& thr : workers) {
            thr = std::thread([&] { worker(); });
        }
    }

    void generate_jobs_and_signal_no_more() {
        produce_jobs();
        jobs_.signal_no_more_elems();
    }

protected:
    void add_job(Job job) { jobs_.push(std::move(job)); }

    virtual void process_job(Job job) = 0;

    virtual void produce_jobs() = 0;

public:
    void run() {
        const int workers_no = std::max(static_cast<int>(std::thread::hardware_concurrency()) - 1,
                1); // One has to remain to prevent deadlock if queue gets full
        std::vector<std::thread> workers(workers_no);
        spawn_workers(workers);
        generate_jobs_and_signal_no_more();
        worker(); // Use current thread to process remaining jobs
        for (std::thread& thr : workers) {
            thr.join();
        }
    }

    JobProcessor() = default;

    JobProcessor(const JobProcessor&) = delete;
    JobProcessor(JobProcessor&&) noexcept = delete;
    JobProcessor& operator=(const JobProcessor&) = delete;
    JobProcessor& operator=(JobProcessor&&) noexcept = delete;

    virtual ~JobProcessor() = default;
};

} // namespace concurrent

#include "dispatcher.hh"
#include "logs.hh"
#include "notify_file.hh"

#include <climits>
#include <cstdint>
#include <future>
#include <map>
#include <poll.h>
#include <queue>
#include <set>
#include <sim/jobs/job.hh>
#include <sim/jobs/utils.hh>
#include <sim/mysql/mysql.hh>
#include <sim/submissions/update_final.hh>
#include <simlib/config_file.hh>
#include <simlib/file_info.hh>
#include <simlib/file_manip.hh>
#include <simlib/process.hh>
#include <simlib/shared_function.hh>
#include <simlib/time.hh>
#include <simlib/working_directory.hh>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <thread>
#include <unistd.h>

#if 0
#define DEBUG_JOB_SERVER(...) __VA_ARGS__
#else
#define DEBUG_JOB_SERVER(...)
#endif

using std::array;
using std::function;
using std::lock_guard;
using std::map;
using std::mutex;
using std::string;
using std::thread;
using std::vector;

namespace job_server {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
thread_local mysql::Connection mysql;

} // namespace job_server

namespace {

class JobsQueue {
public:
    struct Job {
        uint64_t id{};
        uint priority{};
        bool locks_problem = false;

        bool operator<(Job x) const {
            return (priority == x.priority ? id < x.id : priority > x.priority);
        }

        bool operator==(Job x) const { return (id == x.id and priority == x.priority); }

        static constexpr Job least() noexcept { return {UINT64_MAX, 0}; }
    };

private:
    struct ProblemJobs {
        uint64_t problem_id{};
        std::set<Job> jobs;
    };

    struct ProblemInfo {
        Job its_best_job;
        uint locks_no = 0;
    };

    struct {
        // Strong assumption: any job must belong to AT MOST one problem
        std::map<uint64_t, ProblemInfo> problem_info;
        std::map<Job, ProblemJobs> queue; // (best problem's job => all jobs of the problem)
        std::map<int64_t, ProblemJobs> locked_problems; // (problem_id  => (locks, problem's jobs))
    } judge_jobs,
        problem_management_jobs; // (judge jobs - they need judge machines)

    std::set<Job> other_jobs;

    void dump_queues() const {
        auto impl = [](auto&& job_category) {
            if (!job_category.problem_info.empty()) {
                stdlog("problem_info = {");
                for (auto&& [prob_id, pinfo] : job_category.problem_info) {
                    stdlog(
                        "   ", prob_id, " => {", pinfo.its_best_job.id, ", ", pinfo.locks_no, "},"
                    );
                }
                stdlog("}");
            }

            auto log_problem_jobs = [](auto&& logger, const ProblemJobs& pj) {
                logger("{", pj.problem_id, ", {");
                for (auto const& job : pj.jobs) {
                    logger(job.id, " ");
                }
                logger("}}");
            };

            if (!job_category.queue.empty()) {
                stdlog("queue = {");
                for (auto&& [job, problem_jobs] : job_category.queue) {
                    auto tmplog = stdlog("   ", job.id, " => ");
                    log_problem_jobs(tmplog, problem_jobs);
                    tmplog(',');
                };
                stdlog("}");
            }

            if (!job_category.locked_problems.empty()) {
                stdlog("locked_problems = {");
                for (auto&& [prob_id, prob_jobs] : job_category.locked_problems) {
                    auto tmplog = stdlog("   ", prob_id, " => ");
                    log_problem_jobs(tmplog, prob_jobs);
                    tmplog(',');
                };
                stdlog("}");
            }
        };

        stdlog("DEBUG: judge_jobs:");
        impl(judge_jobs);
        stdlog("DEBUG: problem_management_jobs:");
        impl(problem_management_jobs);
    }

public:
    void sync_with_db() {
        STACK_UNWINDING_MARK;

        uint64_t jid = 0;
        EnumVal<sim::jobs::Job::Type> jtype{};
        mysql::Optional<decltype(sim::jobs::Job::aux_id)::value_type> aux_id;
        uint priority = 0;
        InplaceBuff<512> info;
        // Select jobs
        auto stmt = job_server::mysql.prepare("SELECT id, type, priority, aux_id, info "
                                              "FROM jobs "
                                              "WHERE status=? "
                                              "ORDER BY priority DESC, id ASC LIMIT 4");
        stmt.res_bind_all(jid, jtype, priority, aux_id, info);
        // Sets job's status to NOTICED_PENDING
        auto mark_stmt = job_server::mysql.prepare("UPDATE jobs SET status=? WHERE id=?");
        // Add jobs to internal queue
        using JT = sim::jobs::Job::Type;
        for (;;) {
            stmt.bind_and_execute(EnumVal(sim::jobs::Job::Status::PENDING));
            if (not stmt.next()) {
                break;
            }

            do {
                DEBUG_JOB_SERVER(stdlog("DEBUG: Fetched from DB: job ", jid);)
                auto queue_job =
                    [&jid, &priority](auto& job_category, uint64_t problem_id, bool locks_problem) {
                        auto it = job_category.problem_info.find(problem_id);
                        Job curr_job{jid, priority, locks_problem};
                        if (it != job_category.problem_info.end()) {
                            ProblemInfo& pinfo = it->second;
                            Job best_job = pinfo.its_best_job;
                            // Get the problem's jobs (the problem may be locked)
                            auto& pjobs =
                                (pinfo.locks_no > 0 ? job_category.locked_problems[problem_id]
                                                    : job_category.queue[best_job]);
                            // Ensure field 'problem_id' is set properly (in case of
                            // element creation this line is necessary)
                            pjobs.problem_id = problem_id;
                            // Add job to queue
                            pjobs.jobs.emplace(curr_job);
                            // Alter the problem's best job
                            if (curr_job < best_job) {
                                pinfo.its_best_job = curr_job;
                                // Update queue (rekey ProblemJobs)
                                if (pinfo.locks_no == 0) {
                                    auto nh = job_category.queue.extract(best_job);
                                    nh.key() = curr_job;
                                    job_category.queue.insert(std::move(nh));
                                }
                            }

                        } else {
                            job_category.problem_info[problem_id] = {curr_job, 0};
                            // Add job to the queue (first one to this problem)
                            auto& pjobs = job_category.queue[curr_job];
                            pjobs.problem_id = problem_id;
                            pjobs.jobs.emplace(curr_job);
                        }
                    };

                // Assign job to its category
                switch (jtype) {
                case JT::JUDGE_SUBMISSION:
                case JT::REJUDGE_SUBMISSION: {
                    auto opt =
                        str2num<uint64_t>(from_unsafe{sim::jobs::extract_dumped_string(info)});
                    if (not opt) {
                        THROW("Corrupted job's info field");
                    }

                    queue_job(judge_jobs, opt.value(), false);
                    break;
                }

                case JT::ADD_PROBLEM__JUDGE_MODEL_SOLUTION: queue_job(judge_jobs, 0, false); break;

                case JT::REUPLOAD_PROBLEM__JUDGE_MODEL_SOLUTION:
                    queue_job(judge_jobs, aux_id.value(), true);
                    break;

                // Problem job
                case JT::REUPLOAD_PROBLEM:
                case JT::EDIT_PROBLEM:
                case JT::DELETE_PROBLEM:
                case JT::MERGE_PROBLEMS:
                case JT::CHANGE_PROBLEM_STATEMENT:
                case JT::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
                    queue_job(problem_management_jobs, aux_id.value(), true);
                    break;

                // Other job (local jobs that don't have associated problem)
                case JT::ADD_PROBLEM:
                case JT::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
                case JT::MERGE_USERS:
                case JT::DELETE_USER:
                case JT::DELETE_CONTEST:
                case JT::DELETE_CONTEST_ROUND:
                case JT::DELETE_CONTEST_PROBLEM:
                case JT::DELETE_FILE: other_jobs.insert({jid, priority, false}); break;
                }
                mark_stmt.bind_and_execute(EnumVal(sim::jobs::Job::Status::NOTICED_PENDING), jid);
            } while (stmt.next());
        }

        DEBUG_JOB_SERVER(stdlog(__FILE__ ":", __LINE__, ": ", __FUNCTION__, "()");)
        DEBUG_JOB_SERVER(dump_queues();)
    }

    void lock_problem(uint64_t pid) {
        STACK_UNWINDING_MARK;
        DEBUG_JOB_SERVER(stdlog("DEBUG: Locking problem ", pid, "...");)

        auto lock_impl = [&pid](auto& job_category) {
            auto it = job_category.problem_info.find(pid);
            if (it == job_category.problem_info.end()) {
                it = job_category.problem_info.try_emplace(pid, ProblemInfo{Job::least(), 0}).first;
            }

            auto& pinfo = it->second;
            if (++pinfo.locks_no == 1) {
                auto pj = job_category.queue.find(pinfo.its_best_job);
                if (pj != job_category.queue.end()) {
                    job_category.locked_problems.try_emplace(pid, std::move(pj->second));
                    job_category.queue.erase(pj->first);
                }
            }
        };

        lock_impl(judge_jobs);
        lock_impl(problem_management_jobs);

        DEBUG_JOB_SERVER(stdlog(__FILE__ ":", __LINE__, ": ", __FUNCTION__, "()");)
        DEBUG_JOB_SERVER(dump_queues();)
    }

    void unlock_problem(uint64_t pid) {
        STACK_UNWINDING_MARK;
        DEBUG_JOB_SERVER(stdlog("DEBUG: Unlocking problem ", pid, "...");)

        auto unlock_impl = [&pid, this](auto& job_category) {
            auto it = job_category.problem_info.find(pid);
            if (it == job_category.problem_info.end()) {
                dump_queues();
                THROW("BUG: unlocking problem that is not locked!");
            }

            auto& pinfo = it->second;
            if (--pinfo.locks_no == 0) {
                auto pl = job_category.locked_problems.find(pid);
                if (pl != job_category.locked_problems.end()) {
                    job_category.queue.try_emplace(pinfo.its_best_job, std::move(pl->second));
                    job_category.locked_problems.erase(pl->first);
                } else {
                    job_category.problem_info.erase(pid); /* There are no jobs
                        that belong to this problem and it is lock-free now, so
                        its records can be safely removed */
                }
            }
        };

        unlock_impl(judge_jobs);
        unlock_impl(problem_management_jobs);

        DEBUG_JOB_SERVER(stdlog(__FILE__ ":", __LINE__, ": ", __FUNCTION__, "()");)
        DEBUG_JOB_SERVER(dump_queues();)
    }

    class JobHolder {
        JobsQueue* jobs_queue;
        decltype(judge_jobs)* job_category;

    public:
        Job job = Job::least(); // If is invalid it will be the last in comparison
        uint64_t problem_id = 0;

        JobHolder(JobsQueue& jq, decltype(judge_jobs)& jc) : jobs_queue(&jq), job_category(&jc) {}

        JobHolder(JobsQueue& jq, decltype(judge_jobs)& jc, Job j, uint64_t pid)
        : jobs_queue(&jq)
        , job_category(&jc)
        , job(j)
        , problem_id(pid) {}

        JobHolder(const JobHolder&) = delete;
        JobHolder(JobHolder&&) = default;
        JobHolder& operator=(const JobHolder&) = delete;
        JobHolder& operator=(JobHolder&&) = default;
        ~JobHolder() = default;

        // NOLINTNEXTLINE(google-explicit-constructor)
        operator Job() const noexcept { return job; }

        [[nodiscard]] bool ok() const noexcept { return not(job == Job::least()); }

        // Removes the job from queue and locks problem if locks_problem is true
        void was_passed() const {
            STACK_UNWINDING_MARK;

            auto it = job_category->problem_info.find(problem_id);
            if (it == job_category->problem_info.end()) {
                return; // There is no such problem
            }

            auto& pinfo = it->second;
            Job best_job = pinfo.its_best_job;
            auto& pjobs =
                (pinfo.locks_no > 0 ? job_category->locked_problems[problem_id]
                                    : job_category->queue[best_job]);

            if (not pjobs.jobs.erase(job)) {
                THROW("Job erasion did not take place since the erased job was "
                      "not found");
            }

            if (pjobs.jobs.empty()) {
                // That was the last job of it's problem
                if (pinfo.locks_no == 0) {
                    job_category->queue.erase(best_job);
                    job_category->problem_info.erase(problem_id);
                } else { // Problem is locked
                    job_category->locked_problems.erase(problem_id);
                    pinfo.its_best_job = Job::least();
                }

            } else {
                // The best job of the extracted job's problem changed
                Job new_best = *pjobs.jobs.begin();
                pinfo.its_best_job = new_best;
                // Update queue (rekey ProblemJobs)
                if (pinfo.locks_no == 0) {
                    auto nh = job_category->queue.extract(best_job);
                    nh.key() = new_best;
                    job_category->queue.insert(std::move(nh));
                }
            }

            if (job.locks_problem) {
                jobs_queue->lock_problem(problem_id);
            }
        }
    };

private:
    JobHolder best_job(decltype(judge_jobs)& job_category) {
        if (job_category.queue.empty()) {
            return {*this, job_category}; // No one left
        }

        auto it = job_category.queue.begin();
        return {*this, job_category, it->first, it->second.problem_id};
    }

public:
    // Ret val: id of the extracted job or -1 if none was found
    JobHolder best_judge_job() {
        STACK_UNWINDING_MARK;
        return best_job(judge_jobs);
    }

    // Ret val: id of the extracted job or -1 if none was found
    JobHolder best_problem_job() {
        STACK_UNWINDING_MARK;
        return best_job(problem_management_jobs);
    }

    class OtherJobHolder {
        decltype(other_jobs)* oj;

    public:
        Job job = Job::least(); // If is invalid it will be the last in comparison

        // NOLINTNEXTLINE(google-explicit-constructor)
        OtherJobHolder(decltype(other_jobs)& o) : oj(&o) {}

        OtherJobHolder(decltype(other_jobs)& o, Job j) : oj(&o), job(j) {}

        OtherJobHolder(const OtherJobHolder&) = delete;
        OtherJobHolder(OtherJobHolder&&) = default;
        OtherJobHolder& operator=(const OtherJobHolder&) = delete;
        OtherJobHolder& operator=(OtherJobHolder&&) = default;
        ~OtherJobHolder() = default;

        // NOLINTNEXTLINE(google-explicit-constructor)
        operator Job() const noexcept { return job; }

        [[nodiscard]] bool ok() const noexcept { return not(job == Job::least()); }

        // Removes the job from queue
        void was_passed() const { oj->erase(job); }
    };

    // Ret val: id of the extracted job or -1 if none was found
    OtherJobHolder best_other_job() {
        STACK_UNWINDING_MARK;
        if (other_jobs.empty()) {
            return {other_jobs}; // No one left
        }

        return {other_jobs, *other_jobs.begin()};
    }
} jobs_queue; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

class EventsQueue {
    mutex events_lock;
    std::queue<function<void()>> events;
    volatile int notifier_fd_ = -1;

    int set_notifier_fd_impl() {
        if (notifier_fd_ != -1) {
            return notifier_fd_;
        }

        if ((notifier_fd_ = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)) == -1) {
            THROW("eventfd() failed", errmsg());
        }

        return notifier_fd_;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static EventsQueue events_queue;

public:
    static int set_notifier_fd() {
        STACK_UNWINDING_MARK;
        lock_guard<mutex> lock(events_queue.events_lock);
        return events_queue.set_notifier_fd_impl();
    }

    static int get_notifier_fd() { return events_queue.notifier_fd_; }

    static void reset_notifier() {
        lock_guard<mutex> lock(events_queue.events_lock);
        eventfd_t x = 0;
        eventfd_read(events_queue.notifier_fd_, &x);
    }

    template <class Callable>
    static void register_event(Callable&& ev) {
        STACK_UNWINDING_MARK;
        lock_guard<mutex> lock(events_queue.events_lock);
        events_queue.events.push(std::forward<Callable>(ev));
        eventfd_write(events_queue.notifier_fd_, 1);
    }

    /// Return value - a bool denoting whether the event processing took place
    static bool process_next_event() {
        STACK_UNWINDING_MARK;

        function<void()> ev;
        {
            lock_guard<mutex> lock(events_queue.events_lock);
            if (events_queue.events.empty()) {
                return false;
            }

            ev = events_queue.events.front();
            events_queue.events.pop();
        }

        ev();
        return true;
    }
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
EventsQueue EventsQueue::events_queue;

class WorkersPool {
public:
    struct NextJob {
        uint64_t id;
        int64_t problem_id; // negative indicates that no problem is associated
                            // with the job
        bool locked_its_problem;
    };

    struct WorkerInfo {
        NextJob next_job{0, -1, false};
        std::promise<void> next_job_signalizer;
        std::future<void> next_job_waiter = next_job_signalizer.get_future();
        bool is_idle = false;
    };

private:
    mutex mtx_;
    vector<thread::id> idle_workers;
    map<thread::id, WorkerInfo> workers; // AVLDict cannot be used as addresses must not change

    function<void(NextJob)> job_handler_;
    function<void()> worker_becomes_idle_callback_;
    function<void(WorkerInfo)> worker_dies_callback_;

    class Worker {
        WorkersPool& wp_;

    public:
        explicit Worker(WorkersPool& wp) : wp_(wp) {
            STACK_UNWINDING_MARK;
            auto tid = std::this_thread::get_id();
            lock_guard<mutex> lock(wp_.mtx_);
            wp_.workers.emplace(tid, WorkerInfo{});
        }

        Worker(const Worker&) = delete;
        Worker(Worker&&) = delete;
        Worker& operator=(const Worker&) = delete;
        Worker& operator=(Worker&&) = delete;

        ~Worker() {
            STACK_UNWINDING_MARK;
            lock_guard<mutex> lock(wp_.mtx_);
            // Remove the worker from workers map and move out its info
            auto tid = std::this_thread::get_id();
            auto it = wp_.workers.find(tid);
            auto wi = std::move(it->second);
            wp_.workers.erase(it);
            // Remove it from idle workers
            if (wi.is_idle) {
                for (auto k = wp_.idle_workers.begin(); k != wp_.idle_workers.end(); ++k) {
                    if (*k == tid) {
                        wp_.idle_workers.erase(k);
                        break;
                    }
                }
            }

            if (wp_.worker_dies_callback_) {
                EventsQueue::register_event(shared_function([&callback = wp_.worker_dies_callback_,
                                                             winfo = std::move(wi)]() mutable {
                    callback(std::move(winfo));
                }));
            }
        }

        NextJob wait_for_next_job_id() {
            STACK_UNWINDING_MARK;
            auto tid = std::this_thread::get_id();
            WorkerInfo* wi = nullptr;
            // Mark as idle
            {
                lock_guard<mutex> lock(wp_.mtx_);
                wi = &wp_.workers[tid];
                throw_assert(not wi->is_idle);
                wi->is_idle = true;
                wp_.idle_workers.emplace_back(tid);
            }

            if (wp_.worker_becomes_idle_callback_) {
                EventsQueue::register_event(wp_.worker_becomes_idle_callback_);
            }

            // Wait for a job to be assigned to us (current worker)
            wi->next_job_waiter.wait();
            // Rearm promise-future pair
            lock_guard<mutex> lock(wp_.mtx_);
            wi->next_job_signalizer = decltype(wi->next_job_signalizer)();
            wi->next_job_waiter = wi->next_job_signalizer.get_future();

            return wi->next_job;
        }
    };

public:
    /**
     * @brief Construct a workers pool
     *
     * @param job_handler a function that will be called in a worker with a job
     *   id to handle as the first argument
     * @param idle_callback a function to call when a worker becomes idle
     */
    WorkersPool(
        function<void(NextJob)> job_handler,
        function<void()> idle_callback,
        function<void(WorkerInfo)> dead_callback
    )
    : job_handler_(std::move(job_handler))
    , worker_becomes_idle_callback_(std::move(idle_callback))
    , worker_dies_callback_(std::move(dead_callback)) {
        throw_assert(job_handler_);
    }

    void spawn_worker() {
        STACK_UNWINDING_MARK;

        std::thread foo([this] {
            try {
                thread_local Worker w(*this);
                // Connect to databases
                job_server::mysql = sim::mysql::make_conn_with_credential_file(".db.config");

                for (;;) {
                    job_handler_(w.wait_for_next_job_id());
                }

            } catch (const std::exception& e) {
                ERRLOG_CATCH(e);
                // Sleep for a while to prevent exception inundation
                std::this_thread::sleep_for(std::chrono::seconds(3));
                pthread_exit(nullptr);
            }
        });
        foo.detach();
    }

    auto workers_no() {
        lock_guard<mutex> lock(mtx_);
        return workers.size();
    }

    bool pass_job(uint64_t job_id, int64_t problem_id = -1, bool locks_problem = false) {
        STACK_UNWINDING_MARK;

        lock_guard<mutex> lock(mtx_);
        if (idle_workers.empty()) {
            return false;
        }

        // Assign the job to an idle worker
        auto tid = idle_workers.back();
        idle_workers.pop_back();
        auto& wi = workers[tid];
        wi.is_idle = false;
        wi.next_job = {job_id, problem_id, locks_problem};
        wi.next_job_signalizer.set_value();

        return true;
    }
};

} // anonymous namespace

static void spawn_worker(WorkersPool& wp) noexcept {
    try {
        wp.spawn_worker();
    } catch (const std::exception& e) {
        ERRLOG_CATCH(e);
        // Give the system some time (yes I know it will block the whole thread,
        // but currently EventsQueue does not support delaying events)
        std::this_thread::sleep_for(std::chrono::seconds(1));
        EventsQueue::register_event([&] { spawn_worker(wp); });
    }
}

static void process_job(const WorkersPool::NextJob& job) {
    STACK_UNWINDING_MARK;

    auto exit_procedures = [&job] {
        EventsQueue::register_event([job] {
            if (job.locked_its_problem) {
                jobs_queue.unlock_problem(job.problem_id);
            }
        });
    };

    EnumVal<sim::jobs::Job::Type> jtype{};
    mysql::Optional<uint64_t> file_id;
    mysql::Optional<uint64_t> tmp_file_id;
    mysql::Optional<InplaceBuff<32>> creator; // TODO: use uint64_t
    InplaceBuff<32> created_at;
    mysql::Optional<uint64_t> aux_id; // TODO: normalize this column...
    InplaceBuff<512> info;

    {
        auto stmt = job_server::mysql.prepare(
            "SELECT file_id, tmp_file_id, creator, created_at, type, aux_id, info "
            "FROM jobs WHERE id=? AND status!=?"
        );
        stmt.bind_and_execute(job.id, EnumVal(sim::jobs::Job::Status::CANCELED));
        stmt.res_bind_all(file_id, tmp_file_id, creator, created_at, jtype, aux_id, info);

        if (not stmt.next()) { // Job has been probably canceled
            return exit_procedures();
        }
    }

    // Mark as in progress
    job_server::mysql.prepare("UPDATE jobs SET status=? WHERE id=?")
        .bind_and_execute(EnumVal(sim::jobs::Job::Status::IN_PROGRESS), job.id);

    stdlog("Processing job ", job.id, "...");

    std::optional<StringView> creat;
    if (creator.has_value()) {
        creat = creator.value();
    }
    job_server::job_dispatcher(
        job.id, jtype, file_id, tmp_file_id, creat, aux_id, info, created_at
    );

    exit_procedures();
}

static void process_local_job(const WorkersPool::NextJob& job) {
    STACK_UNWINDING_MARK;

    stdlog(
        pthread_self(),
        " got local job {id:",
        job.id,
        ", problem: ",
        job.problem_id,
        ", locked: ",
        job.locked_its_problem,
        '}'
    );

    process_job(job);
}

static void process_judge_job(const WorkersPool::NextJob& job) {
    STACK_UNWINDING_MARK;

    stdlog(
        pthread_self(),
        " got judge job {id:",
        job.id,
        ", problem: ",
        job.problem_id,
        ", locked: ",
        job.locked_its_problem,
        '}'
    );

    process_job(job);
}

static void sync_and_assign_jobs();

static WorkersPool // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    local_workers(process_local_job, sync_and_assign_jobs, [](WorkersPool::WorkerInfo winfo) {
        STACK_UNWINDING_MARK;

        // Job has to be reset and cleanup to be done
        if (not winfo.is_idle) {
            if (winfo.next_job.locked_its_problem) {
                jobs_queue.unlock_problem(winfo.next_job.problem_id);
            }

            sim::jobs::restart_job(
                job_server::mysql, from_unsafe{to_string(winfo.next_job.id)}, false
            );
        }

        EventsQueue::register_event([] { spawn_worker(local_workers); });
    });

static WorkersPool // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    judge_workers(process_judge_job, sync_and_assign_jobs, [](WorkersPool::WorkerInfo winfo) {
        STACK_UNWINDING_MARK;

        // Job has to be reset and cleanup to be done
        if (not winfo.is_idle) {
            if (winfo.next_job.locked_its_problem) {
                jobs_queue.unlock_problem(winfo.next_job.problem_id);
            }

            sim::jobs::restart_job(
                job_server::mysql, from_unsafe{to_string(winfo.next_job.id)}, false
            );
        }

        EventsQueue::register_event([] { spawn_worker(judge_workers); });
    });

static void sync_and_assign_jobs() {
    STACK_UNWINDING_MARK;

    jobs_queue.sync_with_db(); // sync before assigning

    auto problem_job = jobs_queue.best_problem_job();
    auto other_job = jobs_queue.best_other_job();
    auto judge_job = jobs_queue.best_judge_job();

    struct SItem {
        bool ok = false;
        JobsQueue::Job job;
    };

    array<SItem, 3> selector{{
        // (is ok, best job)
        {problem_job.ok(), problem_job},
        {other_job.ok(), other_job},
        {judge_job.ok(), judge_job},
    }};

    while (selector[0].ok or selector[1].ok or selector[2].ok) {
        // Select best job
        auto best_job = JobsQueue::Job::least();
        uint idx = selector.size();
        for (uint i = 0; i < selector.size(); ++i) {
            if (selector[i].ok and selector[i].job < best_job) {
                idx = i;
                best_job = selector[i].job;
            }
        }

        DEBUG_JOB_SERVER(stdlog(
            "DEBUG: Got job: {id: ",
            best_job.id,
            ", pri: ",
            best_job.priority,
            ", locks: ",
            best_job.locks_problem,
            "}"
        ));

        if (idx == 0) { // Problem job
            if (local_workers.pass_job(best_job.id, problem_job.problem_id, best_job.locks_problem))
            {
                problem_job.was_passed();
            } else {
                selector[0].ok = false; // No more workers available
            }

        } else if (idx == 1) { // Other job
            if (local_workers.pass_job(best_job.id)) {
                other_job.was_passed();
            } else {
                selector[1].ok = false; // No more workers available
            }

        } else if (idx == 2) { // Judge job
            if (judge_workers.pass_job(best_job.id, judge_job.problem_id, best_job.locks_problem)) {
                judge_job.was_passed();
            } else {
                selector[2].ok = false; // No more workers available
            }

        } else { // Invalid
            THROW("Invalid idx: ", idx);
        }

        // Update selector (all types must be updated because some problem may
        // become locked causing some jobs to become invalid)
        if (selector[0].ok) {
            problem_job = jobs_queue.best_problem_job();
            selector[0] = {problem_job.ok(), problem_job};
        }
        if (selector[1].ok) {
            other_job = jobs_queue.best_other_job();
            selector[1] = {other_job.ok(), other_job};
        }
        if (selector[2].ok) {
            judge_job = jobs_queue.best_judge_job();
            selector[2] = {judge_job.ok(), judge_job};
        }
    }
}

static void events_loop() noexcept {
    int inotify_fd = -1;
    int inotify_wd = -1;

    // Returns a bool denoting whether inotify is still healthy
    auto process_inotify_event = [&]() -> bool {
        STACK_UNWINDING_MARK;

        ssize_t len = 0;
        char inotify_buff[sizeof(inotify_event) + NAME_MAX + 1];
        for (;;) {
            len = read(inotify_fd, inotify_buff, sizeof(inotify_buff));
            if (len < 1) {
                if (errno == EWOULDBLOCK) {
                    return true; // All has been read
                }

                THROW("read() failed", errmsg());
            }

            // Update jobs queue and distribute new jobs
            sync_and_assign_jobs();

            auto* event = reinterpret_cast<struct inotify_event*>(inotify_buff);
            if (event->mask & IN_MOVE_SELF) {
                // If notify file has been moved
                (void)create_file(job_server::notify_file, S_IRUSR);
                inotify_rm_watch(inotify_fd, inotify_wd);
                inotify_wd = -1;
                return false;
            }
            if (event->mask & IN_IGNORED) {
                // If notify file has disappeared
                (void)create_file(job_server::notify_file, S_IRUSR);
                inotify_wd = -1;
                return false;
            }

            return true;
        }
    };

    for (;;) {
        try {
            STACK_UNWINDING_MARK;
            constexpr uint SLEEP_INTERVAL = 30; // in milliseconds

            // Update jobs queue
            sync_and_assign_jobs();

            // Inotify is broken
            if (inotify_wd == -1) {
                // Try to reset inotify
                if (inotify_fd == -1) {
                    inotify_fd = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
                    if (inotify_fd == -1) {
                        errlog("inotify_init1() failed", errmsg());
                    } else {
                        goto inotify_partially_works;
                    }

                } else {
                    // Try to fix inotify_wd
                inotify_partially_works:
                    // Ensure that notify-file exists
                    if (access(job_server::notify_file, F_OK) == -1) {
                        (void)create_file(job_server::notify_file, S_IRUSR);
                    }

                    inotify_wd = inotify_add_watch(
                        inotify_fd, job_server::notify_file.data(), IN_ATTRIB | IN_MOVE_SELF
                    );
                    if (inotify_wd == -1) {
                        errlog("inotify_add_watch() failed", errmsg());
                    } else {
                        continue; // Fixing was successful
                    }
                }

                /* Inotify is still broken */

                // Try to fix events queue's notifier if it is broken
                if (EventsQueue::get_notifier_fd() == -1) {
                    STACK_UNWINDING_MARK;

                    try {
                        EventsQueue::set_notifier_fd();
                        continue; // Fixing was successful
                    } catch (const std::exception& e) {
                        ERRLOG_CATCH(e);
                    }

                    auto tend = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                    // Events queue is still broken, so process events for
                    // approximately 1 s and try fixing again
                    for (;;) {
                        if (tend < std::chrono::steady_clock::now()) {
                            break;
                        }

                        jobs_queue.sync_with_db();
                        while (EventsQueue::process_next_event()) {
                        }

                        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_INTERVAL));
                    }

                } else {
                    // Events queue is fine
                    STACK_UNWINDING_MARK;

                    pollfd pfd = {EventsQueue::get_notifier_fd(), POLLIN, 0};
                    auto tend = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                    // Process events for 5 seconds
                    for (;;) {
                        if (tend < std::chrono::steady_clock::now()) {
                            break;
                        }

                        // Update queue - sth may have changed
                        sync_and_assign_jobs();

                        int rc = poll(&pfd, 1, SLEEP_INTERVAL);
                        if (rc == -1 and errno != EINTR) {
                            THROW("poll() failed", errmsg());
                        }

                        EventsQueue::reset_notifier();
                        while (EventsQueue::process_next_event()) {
                        }
                    }
                }

            } else {
                // Inotify works well
                if (EventsQueue::get_notifier_fd() == -1) {
                    STACK_UNWINDING_MARK;

                    try {
                        EventsQueue::set_notifier_fd();
                        continue;

                    } catch (const std::exception& e) {
                        ERRLOG_CATCH(e);
                    }

                    /* Events queue is still broken */
                    pollfd pfd = {inotify_fd, POLLIN, 0};
                    auto tend = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                    // Process events for approx 5 seconds
                    for (;;) {
                        if (tend < std::chrono::steady_clock::now()) {
                            break;
                        }

                        int rc = poll(&pfd, 1, SLEEP_INTERVAL);
                        if (rc == -1 and errno != EINTR) {
                            THROW("poll() failed", errmsg());
                        }

                        if (not process_inotify_event()) {
                            continue; // inotify has just broken
                        }

                        while (EventsQueue::process_next_event()) {
                        }
                    }

                } else {
                    // Best scenario - everything works well
                    STACK_UNWINDING_MARK;

                    constexpr uint INFY_IDX = 0;
                    constexpr uint EQ_IDX = 1;
                    pollfd pfd[2] = {
                        {inotify_fd, POLLIN, 0}, {EventsQueue::get_notifier_fd(), POLLIN, 0}
                    };

                    for (;;) {
                        int rc = poll(pfd, 2, -1);
                        if (rc == -1) {
                            if (errno == EINTR) {
                                continue;
                            }

                            THROW("poll() failed", errmsg());
                        }

                        // This should be checked (called) first in so as to
                        // update the jobs queue before processing events
                        if (pfd[INFY_IDX].revents != 0) {
                            if (not process_inotify_event()) {
                                continue; // inotify has just broken
                            }
                        }

                        if (pfd[EQ_IDX].revents != 0) {
                            EventsQueue::reset_notifier();
                            while (EventsQueue::process_next_event()) {
                            }
                        }
                    }
                }
            }

        } catch (const std::exception& e) {
            ERRLOG_CATCH(e);
            // Sleep for a while to prevent exception inundation
            std::this_thread::sleep_for(std::chrono::seconds(8));
        } catch (...) {
            ERRLOG_CATCH();
            // Sleep for a while to prevent exception inundation
            std::this_thread::sleep_for(std::chrono::seconds(8));
        }
    }
}

// Clean up database
static void clean_up_db() {
    STACK_UNWINDING_MARK;

    try {
        // Do it in a transaction to speed things up
        auto transaction = job_server::mysql.start_transaction();

        // Fix jobs that are in progress after the job-server died
        auto stmt = job_server::mysql.prepare("SELECT id, type, info FROM jobs WHERE "
                                              "status=?");
        stmt.bind_and_execute(EnumVal(sim::jobs::Job::Status::IN_PROGRESS));

        decltype(sim::jobs::Job::id) job_id = 0;
        EnumVal<sim::jobs::Job::Type> job_type{};
        InplaceBuff<128> job_info;
        stmt.res_bind_all(job_id, job_type, job_info);
        while (stmt.next()) {
            sim::jobs::restart_job(
                job_server::mysql, from_unsafe{to_string(job_id)}, job_type, job_info, false
            );
        }

        // Fix jobs that are noticed [ending] after the job-server died
        job_server::mysql.prepare("UPDATE jobs SET status=? WHERE status=?")
            .bind_and_execute(
                EnumVal(sim::jobs::Job::Status::PENDING),
                EnumVal(sim::jobs::Job::Status::NOTICED_PENDING)
            );
        transaction.commit();

    } catch (const std::exception& e) {
        ERRLOG_CATCH(e);
    }
}

int main() {
    // Change directory to process executable directory
    try {
        chdir_relative_to_executable_dirpath("..");
    } catch (const std::exception& e) {
        errlog("Failed to change working directory: ", e.what());
        return 1;
    }

    // Terminate older instances
    kill_processes_by_exec({executable_path(getpid())}, std::chrono::seconds(4), true);

    // Loggers
    // stdlog, like everything, writes to stderr, so redirect stdout and stderr
    // to the log file
    if (freopen(job_server::stdlog_file.data(), "ae", stdout) == nullptr ||
        dup3(STDOUT_FILENO, STDERR_FILENO, O_CLOEXEC) == -1)
    {
        errlog("Failed to open `", job_server::stdlog_file, '`', errmsg());
        return 1;
    }

    try {
        errlog.open(job_server::errlog_file);
    } catch (const std::exception& e) {
        errlog("Failed to open `", job_server::errlog_file, "`: ", e.what());
        return 1;
    }

    // Install signal handlers
    struct sigaction sa = {};
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &exit;

    (void)sigaction(SIGINT, &sa, nullptr);
    (void)sigaction(SIGQUIT, &sa, nullptr);
    (void)sigaction(SIGTERM, &sa, nullptr);

    // Connect to the databases
    try {
        job_server::mysql = sim::mysql::make_conn_with_credential_file(".db.config");

        clean_up_db();

        ConfigFile cf;
        cf.add_vars("js_local_workers", "js_judge_workers");
        cf.load_config_from_file("sim.conf");

        size_t lworkers_no = cf["js_local_workers"].as<size_t>().value_or(0);
        if (lworkers_no < 1) {
            THROW("sim.conf: js_local_workers has to be an integer greater "
                  "than 0");
        }

        size_t jworkers_no = cf["js_judge_workers"].as<size_t>().value_or(0);
        if (jworkers_no < 1) {
            THROW("sim.conf: js_judge_workers has to be an integer greater "
                  "than 0");
        }

        // clang-format off
        stdlog("\n=================== Job server launched ==================="
               "\nPID: ", getpid(),
               "\nlocal workers: ", lworkers_no,
               "\njudge workers: ", jworkers_no);
        // clang-format on

        for (size_t i = 0; i < lworkers_no; ++i) {
            spawn_worker(local_workers);
        }

        for (size_t i = 0; i < jworkers_no; ++i) {
            spawn_worker(judge_workers);
        }

    } catch (const std::exception& e) {
        ERRLOG_CATCH(e);
        return 1;
    }

    events_loop();

    return 0;
}

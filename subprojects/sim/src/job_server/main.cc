#include "job_handlers/add_problem.hh"
#include "job_handlers/change_problem_statement.hh"
#include "job_handlers/common.hh"
#include "job_handlers/delete_contest.hh"
#include "job_handlers/delete_contest_problem.hh"
#include "job_handlers/delete_contest_round.hh"
#include "job_handlers/delete_internal_file.hh"
#include "job_handlers/delete_problem.hh"
#include "job_handlers/delete_user.hh"
#include "job_handlers/judge_or_rejudge_submission.hh"
#include "job_handlers/merge_problems.hh"
#include "job_handlers/merge_users.hh"
#include "job_handlers/reselect_final_submissions_in_contest_problem.hh"
#include "job_handlers/reset_problem_time_limits.hh"
#include "job_handlers/reupload_problem.hh"
#include "logs.hh"

#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <fcntl.h>
#include <memory>
#include <pthread.h>
#include <sim/job_server/notify.hh>
#include <sim/jobs/job.hh>
#include <sim/mysql/mysql.hh>
#include <sim/mysql/repeat_if_deadlocked.hh>
#include <sim/sql/sql.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/concurrent/bounded_queue.hh>
#include <simlib/concurrent/mutexed_value.hh>
#include <simlib/config_file.hh>
#include <simlib/errmsg.hh>
#include <simlib/event_queue.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/inotify.hh>
#include <simlib/logger.hh>
#include <simlib/macros/stack_unwinding.hh>
#include <simlib/process.hh>
#include <simlib/repeating.hh>
#include <simlib/syscalls.hh>
#include <simlib/working_directory.hh>
#include <sys/eventfd.h>
#include <sys/signalfd.h>
#include <thread>
#include <unistd.h>

using sim::jobs::Job;
using sim::sql::Select;
using sim::sql::Update;

namespace {

class NewJobFilterBuilder {
private:
    std::map<decltype(Job::id), std::string> job_sql_conditions;

public:
    void add_in_progress_job(
        decltype(Job::id) job_id,
        decltype(Job::type) job_type,
        decltype(Job::aux_id) job_aux_id,
        decltype(Job::aux_id_2) job_aux_id_2
    ) {
        static constexpr auto to_int = [](decltype(Job::type) job_type) {
            return static_cast<decltype(Job::type)::UnderlyingType>(job_type);
        };

        // NOLINTNEXTLINE(bugprone-switch-missing-default-case)
        switch (job_type) {
        case Job::Type::JUDGE_SUBMISSION:
        case Job::Type::REJUDGE_SUBMISSION:
            job_sql_conditions.emplace(
                job_id,
                concat_tostr(
                    "NOT (type IN (",
                    to_int(Job::Type::JUDGE_SUBMISSION),
                    ", ",
                    to_int(Job::Type::REJUDGE_SUBMISSION),
                    ") AND aux_id=",
                    job_aux_id.value(),
                    ")"
                )
            );
            return;

        case Job::Type::ADD_PROBLEM: return; // Does not collide with anything.

        case Job::Type::REUPLOAD_PROBLEM:
        case Job::Type::EDIT_PROBLEM:
        case Job::Type::DELETE_PROBLEM:
        case Job::Type::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
        case Job::Type::CHANGE_PROBLEM_STATEMENT:
            job_sql_conditions.emplace(
                job_id,
                concat_tostr(
                    "NOT ((type IN (",
                    to_int(Job::Type::REUPLOAD_PROBLEM),
                    ", ",
                    to_int(Job::Type::EDIT_PROBLEM),
                    ", ",
                    to_int(Job::Type::DELETE_PROBLEM),
                    ", ",
                    to_int(Job::Type::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION),
                    ", ",
                    to_int(Job::Type::CHANGE_PROBLEM_STATEMENT),
                    ") AND aux_id=",
                    job_aux_id.value(),
                    ") OR (type=",
                    to_int(Job::Type::MERGE_PROBLEMS),
                    " AND (aux_id=",
                    job_aux_id.value(),
                    " OR aux_id_2=",
                    job_aux_id.value(),
                    ")))"
                )
            );
            return;

        case Job::Type::MERGE_PROBLEMS:
            job_sql_conditions.emplace(
                job_id,
                concat_tostr(
                    "NOT ((type IN (",
                    to_int(Job::Type::REUPLOAD_PROBLEM),
                    ", ",
                    to_int(Job::Type::EDIT_PROBLEM),
                    ", ",
                    to_int(Job::Type::DELETE_PROBLEM),
                    ", ",
                    to_int(Job::Type::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION),
                    ", ",
                    to_int(Job::Type::CHANGE_PROBLEM_STATEMENT),
                    ") AND aux_id IN (",
                    job_aux_id.value(),
                    ", ",
                    job_aux_id_2.value(),
                    ")) OR (type=",
                    to_int(Job::Type::MERGE_PROBLEMS),
                    " AND (aux_id IN (",
                    job_aux_id.value(),
                    ", ",
                    job_aux_id_2.value(),
                    ") OR aux_id_2 IN (",
                    job_aux_id.value(),
                    ", ",
                    job_aux_id_2.value(),
                    "))))"
                )
            );
            return;

        case Job::Type::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
        case Job::Type::DELETE_CONTEST_PROBLEM:
            job_sql_conditions.emplace(
                job_id,
                concat_tostr(
                    "NOT (type IN (",
                    to_int(Job::Type::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM),
                    ", ",
                    to_int(Job::Type::DELETE_CONTEST_PROBLEM),
                    ") AND aux_id=",
                    job_aux_id.value(),
                    ")"
                )
            );
            return;

        case Job::Type::DELETE_USER:
            job_sql_conditions.emplace(
                job_id,
                concat_tostr(
                    "NOT ((type=",
                    to_int(Job::Type::DELETE_USER),
                    " AND aux_id=",
                    job_aux_id.value(),
                    ") OR (type=",
                    to_int(Job::Type::MERGE_USERS),
                    " AND (aux_id=",
                    job_aux_id.value(),
                    " OR aux_id_2=",
                    job_aux_id.value(),
                    ")))"
                )
            );
            return;

        case Job::Type::MERGE_USERS:
            job_sql_conditions.emplace(
                job_id,
                concat_tostr(
                    "NOT ((type=",
                    to_int(Job::Type::DELETE_USER),
                    " AND aux_id IN (",
                    job_aux_id.value(),
                    ", ",
                    job_aux_id_2.value(),
                    ")) OR (type=",
                    to_int(Job::Type::MERGE_USERS),
                    " AND (aux_id IN (",
                    job_aux_id.value(),
                    ", ",
                    job_aux_id_2.value(),
                    ") OR aux_id_2 IN (",
                    job_aux_id.value(),
                    ", ",
                    job_aux_id_2.value(),
                    "))))"
                )
            );
            return;

        case Job::Type::DELETE_CONTEST:
            job_sql_conditions.emplace(
                job_id,
                concat_tostr(
                    "NOT (type=",
                    to_int(Job::Type::DELETE_CONTEST),
                    " AND aux_id=",
                    job_aux_id.value(),
                    ")"
                )
            );
            return;

        case Job::Type::DELETE_CONTEST_ROUND:
            job_sql_conditions.emplace(
                job_id,
                concat_tostr(
                    "NOT (type=",
                    to_int(Job::Type::DELETE_CONTEST_ROUND),
                    " AND aux_id=",
                    job_aux_id.value(),
                    ")"
                )
            );
            return;

        case Job::Type::DELETE_INTERNAL_FILE:
            job_sql_conditions.emplace(
                job_id,
                concat_tostr(
                    "NOT (type=",
                    to_int(Job::Type::DELETE_INTERNAL_FILE),
                    " AND aux_id=",
                    job_aux_id.value(),
                    ")"
                )
            );
            return;
        }
        THROW("invalid job_type");
    }

    void remove_in_progress_job(decltype(Job::id) job_id) { job_sql_conditions.erase(job_id); }

    sim::sql::Condition<> build_condition() {
        std::optional<sim::sql::Condition<>> res;
        for (const auto& [job_id, job_sql_condition] : job_sql_conditions) {
            res = std::move(res) &&
                std::optional{sim::sql::Condition(std::string{job_sql_condition})};
        }
        return res.value_or(sim::sql::Condition<>("TRUE"));
    }
};

struct Task {
    decltype(Job::id) job_id;
    decltype(Job::type) job_type;
    decltype(Job::aux_id) job_aux_id;
    decltype(Job::aux_id_2) job_aux_id_2;
};

struct Worker {
    std::thread thread;
    concurrent::BoundedQueue<Task> task_channel{1};
    sim::mysql::Connection mysql = sim::mysql::Connection::from_credential_file(".db.config");
    concurrent::MutexedValue<NewJobFilterBuilder>* new_job_filter_builder = nullptr;
};

enum class CheckStatus { NO_MORE_JOBS_FOR_NOW, MAY_BE_MORE_JOBS };

CheckStatus check_for_and_process_a_new_job(
    sim::mysql::Connection& mysql,
    concurrent::MutexedValue<NewJobFilterBuilder>& new_job_filter_builder,
    concurrent::BoundedQueue<Worker*>& idle_workers
) {
    stdlog("Checking for a new job...");
    decltype(Job::id) id;
    decltype(Job::type) type;
    decltype(Job::aux_id) aux_id;
    decltype(Job::aux_id_2) aux_id_2;
    {
        auto [guard, new_job_filter_builder_value] = new_job_filter_builder.get();

        auto stmt = mysql.execute(Select("id, type, aux_id, aux_id_2")
                                      .from("jobs")
                                      .where(
                                          sim::sql::Condition("status=?", Job::Status::PENDING) &&
                                          new_job_filter_builder_value.build_condition()
                                      )
                                      .order_by("priority DESC, id")
                                      .limit("1"));
        stmt.res_bind(id, type, aux_id, aux_id_2);
        if (!stmt.next()) {
            stdlog("... No new job found.");
            return CheckStatus::NO_MORE_JOBS_FOR_NOW;
        }

        stdlog("... Found job with id: ", id);
        new_job_filter_builder_value.add_in_progress_job(id, type, aux_id, aux_id_2);
    } // Need to destruct the guard to avoid deadlock i.e. waiting for the worker to finish but the
      // worker waits for the guard to be dropped to modify the new_job_filter_builder.
    auto worker = idle_workers.pop();
    // Set job status after we acquire a worker so that number of in-progress jobs is not greater
    // than the number of workers.
    mysql.execute(Update("jobs").set("status=?", Job::Status::IN_PROGRESS).where("id=?", id));
    // Pass the job to the worker.
    worker->task_channel.push(Task{
        .job_id = id,
        .job_type = type,
        .job_aux_id = aux_id,
        .job_aux_id_2 = aux_id_2,
    });
    return CheckStatus::MAY_BE_MORE_JOBS;
}

// This is called in the worker thread to process the job
void process_task(sim::mysql::Connection& mysql, const Task& task) {
    stdlog('[', syscalls::gettid(), "] processing job with id: ", task.job_id);
    job_server::job_handlers::Logger logger;
    try {
        sim::mysql::repeat_if_deadlocked(128, [&] {
            // NOLINTNEXTLINE(bugprone-switch-missing-default-case)
            switch (task.job_type) {
            case Job::Type::JUDGE_SUBMISSION:
            case Job::Type::REJUDGE_SUBMISSION:
                job_server::job_handlers::judge_or_rejudge_submission(
                    mysql, logger, task.job_id, task.job_aux_id.value()
                );
                return;

            case Job::Type::ADD_PROBLEM:
                job_server::job_handlers::add_problem(mysql, logger, task.job_id);
                return;

            case Job::Type::REUPLOAD_PROBLEM:
                job_server::job_handlers::reupload_problem(mysql, logger, task.job_id);
                return;

            case Job::Type::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
                job_server::job_handlers::reselect_final_submissions_in_contest_problem(
                    mysql, logger, task.job_id, task.job_aux_id.value()
                );
                return;

            case Job::Type::DELETE_USER:
                job_server::job_handlers::delete_user(
                    mysql, logger, task.job_id, task.job_aux_id.value()
                );
                return;

            case Job::Type::EDIT_PROBLEM: THROW("not implemented");

            case Job::Type::DELETE_PROBLEM:
                job_server::job_handlers::delete_problem(
                    mysql, logger, task.job_id, task.job_aux_id.value()
                );
                return;

            case Job::Type::DELETE_CONTEST:
                job_server::job_handlers::delete_contest(
                    mysql, logger, task.job_id, task.job_aux_id.value()
                );
                return;

            case Job::Type::DELETE_CONTEST_ROUND:
                job_server::job_handlers::delete_contest_round(
                    mysql, logger, task.job_id, task.job_aux_id.value()
                );
                return;

            case Job::Type::DELETE_CONTEST_PROBLEM:
                job_server::job_handlers::delete_contest_problem(
                    mysql, logger, task.job_id, task.job_aux_id.value()
                );
                return;

            case Job::Type::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
                job_server::job_handlers::reset_problem_time_limits(
                    mysql, logger, task.job_id, task.job_aux_id.value()
                );
                return;

            case Job::Type::MERGE_PROBLEMS:
                job_server::job_handlers::merge_problems(
                    mysql, logger, task.job_id, task.job_aux_id.value(), task.job_aux_id_2.value()
                );
                return;

            case Job::Type::CHANGE_PROBLEM_STATEMENT:
                job_server::job_handlers::change_problem_statement(
                    mysql, logger, task.job_id, task.job_aux_id.value()
                );
                return;

            case Job::Type::MERGE_USERS:
                job_server::job_handlers::merge_users(
                    mysql, logger, task.job_id, task.job_aux_id.value(), task.job_aux_id_2.value()
                );
                return;

            case Job::Type::DELETE_INTERNAL_FILE:
                job_server::job_handlers::delete_internal_file(
                    mysql, logger, task.job_id, task.job_aux_id.value()
                );
                return;
            }
            THROW("invalid task.job_type");
        });
    } catch (const std::exception& e) {
        ERRLOG_CATCH(e);
        job_server::job_handlers::mark_job_as_failed(mysql, logger, task.job_id);
    }
}

} // namespace

int main() {
    // Init server
    // Change directory to process executable directory
    try {
        chdir_relative_to_executable_dirpath("..");
    } catch (const std::exception& e) {
        errlog("Failed to change the working directory: ", e.what());
        return 1;
    }

    // Terminate older instances
    kill_processes_by_exec({executable_path(getpid())}, std::chrono::seconds(4), true);

    // Loggers
    auto open_log_file_as_fd = [&](const char* path, int dest_fd) {
        auto fd = FileDescriptor{path, O_WRONLY | O_APPEND | O_CLOEXEC};
        if (!fd.is_open()) {
            errlog("Failed to open: ", path, errmsg());
            _exit(1);
        }
        if (dup3(fd, dest_fd, O_CLOEXEC) < 0) {
            errlog("dup3()", errmsg());
            _exit(1);
        }
    };
    open_log_file_as_fd(job_server::stdlog_file.c_str(), STDOUT_FILENO);
    stdlog.use(stdout);
    open_log_file_as_fd(job_server::errlog_file.c_str(), STDERR_FILENO);
    errlog.use(stderr);

    // Get the number of worker threads
    ConfigFile config;
    try {
        config.add_vars("job_server_workers");
        config.load_config_from_file("sim.conf");
    } catch (const std::exception& e) {
        errlog("Failed to load sim.conf: ", e.what());
        return 1;
    }
    auto workers_num = config["job_server_workers"].as<size_t>().value_or(0);
    if (workers_num < 1) {
        errlog("sim.conf: Number of job_server_workers has to be an integer greater than 0");
        return 1;
    }

    stdlog(
        "=================== Job server launched ==================="
        "\nPID: ",
        getpid(),
        "\nworkers: ",
        workers_num
    );

    // Block signals so that the worker threads inherit the blocked signal mask
    sigset_t sigset;
    if (sigaddset(&sigset, SIGINT) || sigaddset(&sigset, SIGTERM) || sigaddset(&sigset, SIGQUIT)) {
        errlog("sigaddset()", errmsg());
        return 1;
    }
    if (pthread_sigmask(SIG_BLOCK, &sigset, nullptr)) {
        errlog("pthread_sigmask()", errmsg());
        return 1;
    }

    // Prepare workers
    auto workers = std::vector<Worker>(workers_num);
    auto idle_workers = concurrent::BoundedQueue<Worker*>(workers_num);
    for (auto& worker : workers) {
        idle_workers.push(&worker);
    }

    concurrent::MutexedValue<NewJobFilterBuilder> new_job_filter_builder;

    auto worker_finished_eventfd = FileDescriptor{eventfd(0, EFD_CLOEXEC)};
    if (!worker_finished_eventfd.is_open()) {
        errlog("eventfd()", errmsg());
        return 1;
    }

    // Spawn the workers
    for (auto& worker : workers) {
        worker.thread =
            std::thread{[&idle_workers,
                         &task_channel = worker.task_channel,
                         self = &worker,
                         worker_finished_eventfd = static_cast<int>(worker_finished_eventfd)] {
                for (;;) {
                    auto task = task_channel.pop_opt();
                    if (!task) {
                        break; // No more tasks.
                    }
                    process_task(self->mysql, *task);
                    self->new_job_filter_builder->get().second.remove_in_progress_job(task->job_id);
                    // Signal the main thread that we became idle
                    idle_workers.push(self);
                    uint64_t val = 1;
                    if (write(worker_finished_eventfd, &val, sizeof(val)) != sizeof(val)) {
                        stdlog('[', syscalls::gettid(), "] error: write()", errmsg());
                    }
                }
            }};
        worker.new_job_filter_builder = &new_job_filter_builder;
    }

    auto mysql = sim::mysql::Connection::from_credential_file(".db.config");
    // Restart jobs that were left in-progress by the previous invocation of the job-server.
    mysql.execute(Update("jobs")
                      .set("status=?", Job::Status::PENDING)
                      .where("status=?", Job::Status::IN_PROGRESS));

    FileModificationMonitor file_modification_monitor;
    file_modification_monitor.set_watching_log(std::make_unique<SimpleWatchingLog>());

    bool processing_new_jobs = false;
    auto scheudle_processing_new_jobs = [&mysql,
                                         &idle_workers,
                                         &event_queue = file_modification_monitor.event_queue(),
                                         &processing_new_jobs,
                                         &new_job_filter_builder] {
        if (processing_new_jobs) {
            return;
        }
        processing_new_jobs = true;
        event_queue.add_repeating_handler(
            std::chrono::nanoseconds{0},
            [&mysql, &new_job_filter_builder, &idle_workers, &processing_new_jobs] {
                auto check_status =
                    check_for_and_process_a_new_job(mysql, new_job_filter_builder, idle_workers);
                switch (check_status) {
                case CheckStatus::NO_MORE_JOBS_FOR_NOW: {
                    processing_new_jobs = false;
                    return repeating::STOP;
                }
                case CheckStatus::MAY_BE_MORE_JOBS: return repeating::CONTINUE;
                }
                THROW("invalid check_status");
            }
        );
    };

    // Create the notify file file if does not exist and start watching it
    (void)FileDescriptor{
        sim::job_server::notify_file.to_string(), O_WRONLY | O_TRUNC | O_CREAT | O_CLOEXEC
    };
    file_modification_monitor.add_path(sim::job_server::notify_file.to_string(), true);
    file_modification_monitor.set_event_handler([&](const std::string&) {
        scheudle_processing_new_jobs();
    });
    // Schedule processing new jobs after the notification file starts being watched in case there
    // already are some.
    scheudle_processing_new_jobs();
    // Schedule processing new jobs after the worker finished a job since the job filtering
    // restriction might have relaxed and a new job may become available.
    file_modification_monitor.event_queue()
        .add_file_handler(worker_finished_eventfd, FileEvent::READABLE, [&] {
            // First, consume the readability of the worker_finished_eventfd
            uint64_t val;
            if (read(worker_finished_eventfd, &val, sizeof(val)) != sizeof(val)) {
                errlog("read()", errmsg());
            }
            scheudle_processing_new_jobs();
        });

    // Stop the event queue if a signal arrives
    auto sigfd = FileDescriptor{signalfd(-1, &sigset, SFD_CLOEXEC)};
    if (!sigfd.is_open()) {
        errlog("signalfd()", errmsg());
        return 1;
    }
    file_modification_monitor.event_queue().add_file_handler(sigfd, FileEvent::READABLE, [&] {
        file_modification_monitor.pause_immediately();
    });

    // Run the event loop
    file_modification_monitor.watch();

    stdlog("Shutting down: waiting for the worker threads to finish processing jobs...");
    // Ask worker threads to exit
    for (size_t i = 0; i < workers_num; ++i) {
        auto worker = idle_workers.pop();
        worker->task_channel.signal_no_more_elems();
    }
    // Await worker threads' exit
    for (auto& worker : workers) {
        worker.thread.join();
    }
    stdlog("Job server has shut down.");
}

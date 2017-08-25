#include "judge.h"
#include "problem.h"

#include <future>
#include <map>
#include <poll.h>
#include <queue>
#include <sim/jobs.h>
#include <sim/mysql.h>
#include <sim/sqlite.h>
#include <simlib/avl_dict.h>
#include <simlib/filesystem.h>
#include <simlib/process.h>
#include <simlib/time.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>

using std::function;
using std::lock_guard;
using std::map;
using std::mutex;
using std::pair;
using std::string;
using std::thread;
using std::vector;

thread_local MySQL::Connection mysql;
thread_local SQLite::Connection sqlite;

namespace {

class JobsQueue {
public:
	struct Job {
		uint64_t id;
		uint priority;

		bool operator<(Job x) const {
			return (priority == x.priority ? id < x.id : priority > x.priority);
		}

		bool operator==(Job x) const {
			return (id == x.id and priority == x.priority);
		}
	};

private:
	struct ProblemJobs {
		uint64_t problem_id;
		AVLDictSet<Job> jobs;
	};

	struct {
		// Strong assumption: any job must belong to AT MOST one problem
		AVLDictMap<uint64_t, Job> problem_to_its_best_job;
		AVLDictMap<Job, ProblemJobs> queue; /*
			(best problem's job => all jobs of the problem) */
		AVLDictMap<int64_t, pair<uint, ProblemJobs>> locked_problems; /*
			(problem_id  => (locks, problem's jobs)) */
	} judge_jobs, problem_jobs; // (judge jobs - they need judge machines)

	AVLDictSet<Job> other_jobs;

public:
	void sync_with_db() {
		STACK_UNWINDING_MARK;

		uint64_t jid;
		uint jtype_u;
		uint64_t aux_id;
		uint priority;
		InplaceBuff<512> info;
		// Select jobs
		auto stmt = mysql.prepare("SELECT id, type, priority, aux_id, info"
			" FROM jobs"
			" WHERE status=" JSTATUS_PENDING_STR " AND type!=" JTYPE_VOID_STR
			" ORDER BY priority DESC LIMIT 4");
		stmt.res_bind_all(jid, jtype_u, priority, aux_id, info);
		// Sets job's status to NOTICED_PENDING
		auto mark_stmt = mysql.prepare("UPDATE jobs SET status="
			JSTATUS_NOTICED_PENDING_STR " WHERE id=?");
		mark_stmt.bind_all(jid);
		// Add jobs to internal queue
		using JT = JobType;
		for (stmt.execute(); stmt.next(); stmt.execute())
			do {
				auto queue_job =
					[&jid, &priority](auto& job_category, uint64_t problem_id)
				{
					auto it =
						job_category.problem_to_its_best_job.find(problem_id);
					Job curr_job {jid, priority};
					if (it) {
						Job best_job = it->second;
						// Get the problem's jobs (the problem may be locked)
						auto lit =
							job_category.locked_problems.find(problem_id);
						auto& pjobs = (lit ? lit->second.second
							: job_category.queue[best_job]);
						// Add job to queue
						pjobs.jobs.emplace(curr_job);
						// Alter the problem's best job
						if (best_job < curr_job) {
							it->second = curr_job;
							if (not lit) // Update queue (rekey ProblemJobs)
								job_category.queue.alter_key(best_job, curr_job);
						}

					} else {
						job_category.problem_to_its_best_job[problem_id] =
							curr_job;
						// Add job to the queue (first one to this problem)
						auto& pjobs = job_category.queue[curr_job];
						pjobs.problem_id = problem_id;
						pjobs.jobs.emplace(curr_job);
					}
				};

				// Assign job to its category
				switch (JobType(jtype_u)) {
				// Judge job
				case JT::JUDGE_SUBMISSION:
					queue_job(judge_jobs,
						strtoull(jobs::extractDumpedString(info)));
					break;

				case JT::ADD_JUDGE_MODEL_SOLUTION:
				case JT::REUPLOAD_JUDGE_MODEL_SOLUTION:
					// TODO: this is a bit more tricky than the above
					break;

				// Problem job
				case JT::REUPLOAD_PROBLEM:
				case JT::EDIT_PROBLEM:
				case JT::DELETE_PROBLEM:
					queue_job(problem_jobs, aux_id);
					break;

				// Other job
				case JT::ADD_PROBLEM:
					other_jobs.emplace(jid, priority);
					break;

				case JT::VOID:
					throw_assert(false);
				}
				mark_stmt.execute();
			} while (stmt.next());
	}

	void lock_problem(uint64_t pid) {
		STACK_UNWINDING_MARK;

		auto lock_impl = [&pid](auto& job_category) {
			auto it = job_category.problem_to_its_best_job.find(pid);
			if (not it)
				return;

			auto pj = job_category.queue.find(it->second);
			if (pj) {
				job_category.locked_problems.insert(pid,
					{1, std::move(pj->second)});
				job_category.queue.erase(it->second);
			} else
				++job_category.locked_problems[pid].first;
		};

		lock_impl(judge_jobs);
		lock_impl(problem_jobs);
	}

	void unlock_problem(uint64_t pid) {
		STACK_UNWINDING_MARK;

		auto unlock_impl = [&pid](auto& job_category) {
			auto pl = job_category.locked_problems.find(pid);
			if (pl and --pl->second.first == 0) {
				auto best_job = job_category.problem_to_its_best_job[pid];
				job_category.queue.emplace(best_job,
					std::move(pl->second.second));
				job_category.locked_problems.erase(pid);
			}
		};

		unlock_impl(judge_jobs);
		unlock_impl(problem_jobs);
	}

	class JobHolder {
		decltype(judge_jobs)* job_category;

	public:
		int64_t job_id = -1;
		uint priority = 0;
		uint64_t problem_id = 0;

		JobHolder(decltype(judge_jobs)& jc): job_category(&jc) {}

		JobHolder(decltype(judge_jobs)& jc, Job j, uint64_t pid)
			: job_category(&jc), job_id(j.id), priority(j.priority),
			problem_id(pid) {}

		operator Job() const noexcept { return {(uint64_t)job_id, priority}; }

		bool ok() const noexcept { return (job_id >= 0); }

		void remove() const {
			auto pit = job_category->problem_to_its_best_job.find(problem_id);
			if (not pit)
				return; // There is no such problem

			Job best_job = pit->second;
			auto& pjobs = job_category->queue[best_job];
			pjobs.jobs.erase({(uint64_t)job_id, priority});
			// That was the last job of it's problem
			if (pjobs.jobs.empty()) {
				job_category->problem_to_its_best_job.erase(problem_id);
				job_category->queue.erase(best_job);
			// The best job of the extracted job's problem changed
			} else {
				Job new_best = *pjobs.jobs.front();
				job_category->problem_to_its_best_job[problem_id] = new_best;
				// Update queue (rekey ProblemJobs)
				job_category->queue.alter_key(best_job, new_best);
			}
		}
	};

private:
	JobHolder best_job(decltype(judge_jobs)& job_category) {
		if (job_category.queue.empty())
			return {job_category}; // No one left

		auto it = job_category.queue.front();
		return {job_category, it->first, it->second.problem_id};
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
		return best_job(problem_jobs);
	}

	class OtherJobHolder {
		decltype(other_jobs)* oj;

	public:
		int64_t job_id = -1;
		uint priority = 0;

		operator Job() const noexcept { return {(uint64_t)job_id, priority}; }

		bool ok() const noexcept { return (job_id >= 0); }

		OtherJobHolder(decltype(other_jobs)& o): oj(&o) {}

		OtherJobHolder(decltype(other_jobs)& o, Job j)
			: oj(&o), job_id(j.id), priority(j.priority) {}

		void remove() const {
			oj->erase({(uint64_t)job_id, priority});
		}
	};

	// Ret val: id of the extracted job or -1 if none was found
	OtherJobHolder best_other_job() {
		STACK_UNWINDING_MARK;
		if (other_jobs.empty())
			return {other_jobs}; // No one left

		return {other_jobs, *other_jobs.front()};
	}
} jobs_queue;

class EventsQueue {
	mutex events_lock;
	std::queue<function<void()>> events;
	volatile int notifier_fd_ = -1;

	int set_notifier_fd_impl() {
		if (notifier_fd_ != -1)
			return notifier_fd_;

		if ((notifier_fd_ = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)) == -1)
			THROW("eventfd() failed", error(errno));

		return notifier_fd_;
	}

	static EventsQueue eq;

public:
	static int set_notifier_fd() {
		STACK_UNWINDING_MARK;
		lock_guard<mutex> lock(eq.events_lock);
		return eq.set_notifier_fd_impl();
	}

	static int get_notifier_fd() { return eq.notifier_fd_; }

	static void reset_notifier() {
		lock_guard<mutex> lock(eq.events_lock);
		eventfd_t x;
		eventfd_read(eq.notifier_fd_, &x);
	}

	template<class Callable>
	static void register_event(Callable&& ev) {
		STACK_UNWINDING_MARK;
		lock_guard<mutex> lock(eq.events_lock);
		eq.events.push(std::forward<Callable>(ev));
		eventfd_write(eq.notifier_fd_, 1);
	}

	/// Return value - a bool denoting whether the event processing took place
	static bool process_next_event() {
		STACK_UNWINDING_MARK;

		function<void()> ev;
		{
			lock_guard<mutex> lock(eq.events_lock);
			if (eq.events.empty())
				return false;

			ev = eq.events.front();
			eq.events.pop();
		}

		ev();
		return true;
	}
};

EventsQueue EventsQueue::eq;

class WorkersPool {
public:
	struct NextJob {
		uint64_t id;
		int64_t problem_id; // negative indicates that no problem is associated
		                    // with the job
	};

	struct WorkerInfo {
		NextJob next_job {0, -1};
		std::promise<void> next_job_signalizer;
		std::future<void> next_job_waiter = next_job_signalizer.get_future();
		bool is_idle = false;
	};

private:
	mutex mtx_;
	vector<thread::id> idle_workers;
	map<thread::id, WorkerInfo> workers; // AVLDict cannot be used as addresses
	                                     // must not change

	function<void(NextJob)> job_handler_;
	function<void()> worker_becomes_idle_callback_;
	function<void(WorkerInfo)> worker_dies_callback_;

	class Worker {
		WorkersPool& wp_;

	public:
		Worker(WorkersPool& wp) : wp_(wp) {
			STACK_UNWINDING_MARK;
			auto tid = std::this_thread::get_id();
			lock_guard<mutex> lock(wp_.mtx_);
			wp_.workers.emplace(tid, WorkerInfo{});
		}

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
				for (auto k = wp_.idle_workers.begin();
					k != wp_.idle_workers.end(); ++k)
				{
					if (*k == tid) {
						wp_.idle_workers.erase(k);
						break;
					}
				}
			}

			if (wp_.worker_dies_callback_)
				EventsQueue::register_event(make_shared_function(
					[&callback = wp_.worker_dies_callback_,
						winfo = std::move(wi)]() mutable
					{
						callback(std::move(winfo));
					}));
		}

		NextJob wait_for_next_job_id() {
			STACK_UNWINDING_MARK;
			auto tid = std::this_thread::get_id();
			WorkerInfo* wi;
			// Mark as idle
			{
				lock_guard<mutex> lock(wp_.mtx_);
				wi = &wp_.workers[tid];
				throw_assert(not wi->is_idle);
				wi->is_idle = true;
				wp_.idle_workers.emplace_back(tid);
			}

			if (wp_.worker_becomes_idle_callback_)
				EventsQueue::register_event(wp_.worker_becomes_idle_callback_);

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
	WorkersPool(function<void(NextJob)> job_handler,
			function<void()> idle_callback,
			function<void(WorkerInfo)> dead_callback)
		: job_handler_(std::move(job_handler)),
			worker_becomes_idle_callback_(std::move(idle_callback)),
			worker_dies_callback_(std::move(dead_callback))
	{
		throw_assert(job_handler_);
	}

	void spawn_worker() {
		STACK_UNWINDING_MARK;

		std::thread foo([this]{
			thread_local Worker w(*this);
			for (;;)
				job_handler_(w.wait_for_next_job_id());
		});
		foo.detach();
	}

	auto workers_no() {
		lock_guard<mutex> lock(mtx_);
		return workers.size();
	}

	bool pass_job(uint64_t job_id, int64_t problem_id = -1) {
		STACK_UNWINDING_MARK;

		lock_guard<mutex> lock(mtx_);
		if (idle_workers.size() == 0)
			return false;

		// Assign the job to an idle worker
		auto tid = idle_workers.back();
		idle_workers.pop_back();
		auto& wi = workers[tid];
		wi.is_idle = false;
		wi.next_job = {job_id, problem_id};
		wi.next_job_signalizer.set_value();

		return true;
	}
};

} // anonymous namespace

static void assign_jobs();

static void spawn_worker(WorkersPool& wp) noexcept {
	try {
		wp.spawn_worker();
	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		// Give the system some time (yes I know it will block the whole thread,
		// but currently EventsQueue does not support delaying events)
		std::this_thread::sleep_for(std::chrono::seconds(1));
		EventsQueue::register_event([&]{ spawn_worker(wp); });
	}
}

static WorkersPool local_workers([](WorkersPool::NextJob job) {
		STACK_UNWINDING_MARK;

		mysql = MySQL::makeConnWithCredFile(".db.config");
		// auto stmt = mysql.prepare("UPDATE jobs SET status=" JSTATUS_IN_PROGRESS_STR " WHERE id=?");
		auto stmt = mysql.prepare("UPDATE jobs SET status=" JSTATUS_PENDING_STR " WHERE id=?");
		stmt.bindAndExecute(job.id);
		stdlog(pthread_self(), " got local {", job.id, ", ", job.problem_id, '}');
		std::this_thread::sleep_for(std::chrono::seconds(3));
		pthread_exit(nullptr);

		EventsQueue::register_event([job]{
			if (job.problem_id >= 0)
				jobs_queue.unlock_problem(job.problem_id);
		});

	}, assign_jobs, [](WorkersPool::WorkerInfo winfo) {
		STACK_UNWINDING_MARK;

		// Job has to be reset and cleanup done
		if (not winfo.is_idle) {
			if (winfo.next_job.problem_id >= 0)
				jobs_queue.unlock_problem(winfo.next_job.problem_id);

			jobs::restart_job(mysql, toStr(winfo.next_job.id), false);
		}

		EventsQueue::register_event([]{
			spawn_worker(local_workers);
		});
	});

static WorkersPool judge_workers([](WorkersPool::NextJob job) {
		STACK_UNWINDING_MARK;

		mysql = MySQL::makeConnWithCredFile(".db.config");
		auto stmt = mysql.prepare("UPDATE jobs SET status=" JSTATUS_IN_PROGRESS_STR " WHERE id=?");
		stmt.bindAndExecute(job.id);
		stdlog(pthread_self(), " got judge {", job.id, ", ", job.problem_id, '}');
		std::this_thread::sleep_for(std::chrono::seconds(3));
		pthread_exit(nullptr);

	}, assign_jobs, [](WorkersPool::WorkerInfo winfo) {
		STACK_UNWINDING_MARK;

		// Job has to be reset and cleanup done
		if (not winfo.is_idle)
			jobs::restart_job(mysql, toStr(winfo.next_job.id), false);

		EventsQueue::register_event([]{
			spawn_worker(judge_workers);
		});
	});

static void assign_jobs() {
	STACK_UNWINDING_MARK;

	jobs_queue.sync_with_db(); // sync before assigning

	auto problem_job = jobs_queue.best_problem_job();
	auto other_job = jobs_queue.best_other_job();
	auto judge_job = jobs_queue.best_judge_job();

	// TODO: maybe just two ifs instead of an AVL tree?
	AVLDictMap<JobsQueue::Job, function<void()>> selector; // job =>
	                                             // (job handler, update record)

	if (problem_job.ok())
		selector.emplace(problem_job, [&] {
				if (local_workers.pass_job(problem_job.job_id,
					problem_job.problem_id))
				{
					problem_job.remove(); // Job has been passed
					jobs_queue.lock_problem(problem_job.problem_id);
					// Update selector
					auto old_key = problem_job;
					problem_job = jobs_queue.best_problem_job();
					if (problem_job.ok())
						selector.alter_key(old_key, problem_job);
					else
						selector.erase(old_key);
				} else
					selector.erase(problem_job); // No worker to handle the job
			});

	if (other_job.ok())
		selector.emplace(other_job, [&] {
				if (local_workers.pass_job(other_job.job_id)) {
					other_job.remove(); // Job has been passed
					// Update selector
					auto old_key = other_job;
					other_job = jobs_queue.best_other_job();
					if (other_job.ok())
						selector.alter_key(old_key, other_job);
					else
						selector.erase(old_key);
				} else
					selector.erase(other_job); // No worker to handle the job
			});
	using Job = JobsQueue::Job;

	if (judge_job.ok())
		selector.emplace(judge_job, [&] {
				if (judge_workers.pass_job(judge_job.job_id,
					judge_job.problem_id))
				{
					judge_job.remove(); // Job has been passed
					// Update selector
					Job old_key = judge_job.operator Job();
					judge_job = jobs_queue.best_judge_job();
					if (judge_job.ok())
						selector.alter_key(old_key, judge_job);
					else
						selector.erase(old_key);
				} else
					selector.erase(judge_job); // No worker to handle the job
			});

	while (selector.size())
		selector.front()->second();
}

static void eventsLoop() noexcept {
	int inotify_fd = -1;
	int inotify_wd = -1;

	// Returns a bool denoting whether inotify is still healthy
	auto process_inotify_event = [&]() -> bool {
		STACK_UNWINDING_MARK;

		ssize_t len;
		char inotify_buff[sizeof(inotify_event) + NAME_MAX + 1];
		for (;;) {
			len = read(inotify_fd, inotify_buff, sizeof(inotify_buff));
			if (len < 1) {
				if (errno == EWOULDBLOCK)
					return true; // All has been read

				THROW("read() failed", error(errno));
			}

			// Update jobs queue and distribute new jobs
			jobs_queue.sync_with_db();
			assign_jobs();

			struct inotify_event *event = (struct inotify_event *) inotify_buff;
			// If notify file has been moved
			if (event->mask & IN_MOVE_SELF) {
				(void)createFile(JOB_SERVER_NOTIFYING_FILE, S_IRUSR);
				inotify_rm_watch(inotify_fd, inotify_wd);
				inotify_wd = -1;
				return false;

			// If notify file has disappeared
			} else if (event->mask & IN_IGNORED) {
				(void)createFile(JOB_SERVER_NOTIFYING_FILE, S_IRUSR);
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
			jobs_queue.sync_with_db();
			assign_jobs();

			// Inotify is broken
			if (inotify_wd == -1) {
				// Try to reset inotify
				if (inotify_fd == -1) {
					inotify_fd = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
					if (inotify_fd == -1)
						errlog("inotify_init1() failed", error(errno));
					else
						goto inotify_partially_works;

				// Try to fix inotify_wd
				} else {
				inotify_partially_works:
					// Ensure that notify-file exists
					if (access(JOB_SERVER_NOTIFYING_FILE, F_OK) == -1)
						(void)createFile(JOB_SERVER_NOTIFYING_FILE, S_IRUSR);

					inotify_wd = inotify_add_watch(inotify_fd,
						JOB_SERVER_NOTIFYING_FILE, IN_ATTRIB | IN_MOVE_SELF);
					if (inotify_wd == -1)
						errlog("inotify_add_watch() failed", error(errno));
					else
						continue; // Fixing was successful
				}

				/* Inotify is still broken */

				// Try to fix events queue's notifier if it is broken
				if (EventsQueue::get_notifier_fd() == -1) {
					try {
						EventsQueue::set_notifier_fd();
						continue; // Fixing was successful
					} catch (const std::exception& e) {
						ERRLOG_CATCH(e);
					}

					auto tend = std::chrono::steady_clock::now() +
						std::chrono::seconds(5);
					// Events queue is still broken, so process events for
					// approximately 1 s and try fixing again
					for (;;) {
						if (tend < std::chrono::steady_clock::now())
							break;

						jobs_queue.sync_with_db();
						while (EventsQueue::process_next_event()) {}

						std::this_thread::sleep_for(
							std::chrono::milliseconds(SLEEP_INTERVAL));
					}

				// Events queue is fine
				} else {
					pollfd pfd = {EventsQueue::get_notifier_fd(), POLLIN, 0};
					auto tend = std::chrono::steady_clock::now() +
						std::chrono::seconds(5);
					// Process events for 5 seconds
					for (;;) {
						if (tend < std::chrono::steady_clock::now())
							break;

						// Update queue - sth may have changed
						jobs_queue.sync_with_db();
						assign_jobs();

						int rc = poll(&pfd, 1, SLEEP_INTERVAL);
						if (rc == -1)
							THROW("poll() failed", error(errno));

						EventsQueue::reset_notifier();
						while (EventsQueue::process_next_event()) {}
					}
				}

			// Inotify is working well
			} else {
				if (EventsQueue::get_notifier_fd() == -1) {
					try {
						EventsQueue::set_notifier_fd();
						continue;

					} catch (const std::exception& e) {
						ERRLOG_CATCH(e);
					}

					/* Events queue is still broken */
					pollfd pfd = {inotify_fd, POLLIN, 0};
					auto tend = std::chrono::steady_clock::now() +
						std::chrono::seconds(5);
					// Process events for approx 5 seconds
					for (;;) {
						if (tend < std::chrono::steady_clock::now())
							break;

						int rc = poll(&pfd, 1, SLEEP_INTERVAL);
						if (rc == -1)
							THROW("poll() failed", error(errno));

						if (not process_inotify_event())
							continue; // inotify has just broken

						while (EventsQueue::process_next_event()) {}
					}


				// Best scenario - everything works well
				} else {
					constexpr uint INFY_IDX = 0;
					constexpr uint EQ_IDX = 1;
					pollfd pfd[2] = {
						{inotify_fd, POLLIN, 0},
						{EventsQueue::get_notifier_fd(), POLLIN, 0}
					};

					for (;;) {
						int rc = poll(pfd, 2, -1);
						if (rc == -1)
							THROW("poll() failed", error(errno));

						// This should be checked (called) first in so as to
						// update the jobs queue before processing events
						if (pfd[INFY_IDX].revents != 0)
							if (not process_inotify_event())
								continue; // inotify has just broken

						if (pfd[EQ_IDX].revents != 0) {
							EventsQueue::reset_notifier();
							while (EventsQueue::process_next_event()) {}
						}
					}
				}
			}

		} catch (const std::exception& e) {
			ERRLOG_CATCH(e);
			// Sleep for a while to prevent exception inundation
			std::this_thread::sleep_for(std::chrono::seconds(1));
		} catch (...) {
			ERRLOG_CATCH();
			// Sleep for a while to prevent exception inundation
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}
}

#if 0
static void processJobQueue() noexcept {
	// While job queue is not empty
	for (;;) try {
		STACK_UNWINDING_MARK;

		MySQL::Result res = mysql.query(
			"SELECT id, type, aux_id, info, creator, added FROM jobs"
			" WHERE status=" JSTATUS_PENDING_STR " AND type!=" JTYPE_VOID_STR
			" ORDER BY priority DESC, id LIMIT 1");

		if (!res.next())
			break;

		StringView job_id = res[0];
		JobType type = JobType(strtoull(res[1]));
		StringView aux_id = (res.is_null(2) ? "" : res[2]);
		StringView info = res[3];

		// Change the job status to IN_PROGRESS
		auto stmt = mysql.prepare("UPDATE jobs"
			" SET status=" JSTATUS_IN_PROGRESS_STR " WHERE id=?");
		stmt.bindAndExecute(job_id);

		// Choose action depending on the job type
		switch (type) {
		case JobType::JUDGE_SUBMISSION:
			judgeSubmission(job_id, aux_id, res[5]);
			break;

		case JobType::ADD_PROBLEM:
			addProblem(job_id, res[4], info);
			break;

		case JobType::REUPLOAD_PROBLEM:
			reuploadProblem(job_id, res[4], info, aux_id);
			break;

		case JobType::ADD_JUDGE_MODEL_SOLUTION:
			judgeModelSolution(job_id, JobType::ADD_PROBLEM);
			break;

		case JobType::REUPLOAD_JUDGE_MODEL_SOLUTION:
			judgeModelSolution(job_id, JobType::REUPLOAD_PROBLEM);
			break;

		case JobType::EDIT_PROBLEM:
		case JobType::DELETE_PROBLEM:
			stmt = mysql.prepare("UPDATE jobs"
				" SET status=" JSTATUS_CANCELED_STR " WHERE id=?");
			stmt.bindAndExecute(job_id);
			break;

		case JobType::VOID:
			break;

		}

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		// Give up for a couple of seconds to not litter the error log
		usleep(3e6);

	} catch (...) {
		ERRLOG_CATCH();
		// Give up for a couple of seconds to not litter the error log
		usleep(3e6);
	}
}
#endif

// Clean up database
static void cleanUpDBs() {
	try {
		// Fix jobs that are in progress after the job-server died
		auto res = mysql.query("SELECT id, type, info FROM jobs WHERE status="
			JSTATUS_IN_PROGRESS_STR);
		while (res.next())
			jobs::restart_job(mysql, res[0], JobType(strtoull(res[1])), res[2],
				false);

		// Fix jobs that are noticed [ending] after the job-server died
		mysql.update("UPDATE jobs SET status=" JSTATUS_PENDING_STR
			" WHERE status=" JSTATUS_NOTICED_PENDING_STR);

		// Remove void (invalid) jobs and submissions that are older than 24 h
		auto yesterday_date = date("%Y-%m-%d %H:%M:%S",
			time(nullptr) - 24 * 60 * 60);
		auto stmt = mysql.prepare("DELETE FROM jobs WHERE type="
			JTYPE_VOID_STR " AND added<?");
		stmt.bindAndExecute(yesterday_date);

		stmt = mysql.prepare("DELETE FROM submissions WHERE type="
			STYPE_VOID_STR " AND submit_time<?");
		stmt.bindAndExecute(yesterday_date);

		// Remove void (invalid) problems and associated with them submissions
		// and jobs
		for (;;) { // TODO test it
			res = mysql.query("SELECT id FROM problems"
				" WHERE type=" PTYPE_VOID_STR " LIMIT 32");
			if (res.rows_num() == 0)
				break;

			while (res.next()) {
				StringView pid = res[1];
				// Delete submissions
				stmt = mysql.prepare("DELETE FROM submissions"
					" WHERE problem_id=?");
				stmt.bindAndExecute(pid);

				// Delete problem's files
				(void)remove_r(StringBuff<PATH_MAX>{"problems/", pid});
				(void)remove(StringBuff<PATH_MAX>{"problems/", pid, ".zip"});

				// Delete the problem (we have to do it here to prevent this
				// loop from going infinite)
				stmt = mysql.prepare("DELETE FROM problems WHERE id=?");
				stmt.bindAndExecute(pid);
			}
		}


	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
	}
}

int main() {
	// Change directory to process executable directory
	string cwd;
	try {
		cwd = chdirToExecDir();
	} catch (const std::exception& e) {
		errlog("Failed to change working directory: ", e.what());
	}

	// Loggers
	// stdlog, like everything, writes to stderr, so redirect stdout and stderr
	// to the log file
	if (freopen(JOB_SERVER_LOG, "a", stdout) == nullptr ||
		dup2(STDOUT_FILENO, STDERR_FILENO) == -1)
	{
		errlog("Failed to open `", JOB_SERVER_LOG, '`', error(errno));
	}

	try {
		errlog.open(JOB_SERVER_ERROR_LOG);
	} catch (const std::exception& e) {
		errlog("Failed to open `", JOB_SERVER_ERROR_LOG, "`: ", e.what());
	}

	stdlog("\n=================== Job server launched ===================\n"
		"PID: ", getpid());

	// Install signal handlers
	struct sigaction sa;
	memset (&sa, 0, sizeof(sa));
	sa.sa_handler = &exit;

	(void)sigaction(SIGINT, &sa, nullptr);
	(void)sigaction(SIGQUIT, &sa, nullptr);
	(void)sigaction(SIGTERM, &sa, nullptr);

	// Connect to the databases
	try {
		sqlite = SQLite::Connection(SQLITE_DB_FILE, SQLITE_OPEN_READWRITE |
			SQLITE_OPEN_NOMUTEX);
		mysql = MySQL::makeConnWithCredFile(".db.config");

		cleanUpDBs();

		for (int i = 0; i < 4; ++i)
			spawn_worker(local_workers);

		for (int i = 0; i < 4; ++i)
			spawn_worker(judge_workers);

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return 1;
	}

	eventsLoop();

	return 0;
}

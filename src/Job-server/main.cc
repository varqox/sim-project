#include "dispatcher.h"

#include <future>
#include <map>
#include <poll.h>
#include <queue>
#include <sim/jobs.h>
#include <sim/mysql.h>
#include <sim/submission.h>
#include <simlib/avl_dict.h>
#include <simlib/config_file.h>
#include <simlib/filesystem.h>
#include <simlib/process.h>
#include <simlib/time.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>

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
using std::pair;
using std::string;
using std::thread;
using std::vector;

thread_local MySQL::Connection mysql;

namespace {

class JobsQueue {
public:
	struct Job {
		uint64_t id;
		uint priority;
		bool locks_problem = false;

		bool operator<(Job x) const {
			return (priority == x.priority ? id < x.id : priority > x.priority);
		}

		bool operator==(Job x) const {
			return (id == x.id and priority == x.priority);
		}

		constexpr static Job least() noexcept { return {UINT64_MAX, 0}; }
	};

private:
	struct ProblemJobs {
		uint64_t problem_id;
		AVLDictSet<Job> jobs;
	};

	struct ProblemInfo {
		Job its_best_job;
		uint locks_no = 0;
	};

	struct {
		// Strong assumption: any job must belong to AT MOST one problem
		AVLDictMap<uint64_t, ProblemInfo> problem_info;
		AVLDictMap<Job, ProblemJobs>
		   queue; // (best problem's job => all jobs of the problem)
		AVLDictMap<int64_t, ProblemJobs>
		   locked_problems; // (problem_id  => (locks, problem's jobs))
	} judge_jobs, problem_jobs; // (judge jobs - they need judge machines)

	AVLDictSet<Job> other_jobs;

	void dump_queues() const {
		auto impl = [](auto&& job_category) {
			if (job_category.problem_info.size()) {
				stdlog("problem_info = {");
				job_category.problem_info.for_each([](auto&& p) {
					stdlog("   ", p.first, " => {", p.second.its_best_job.id,
					       ", ", p.second.locks_no, "},");
				});
				stdlog("}");
			}

			auto log_problem_job = [](auto&& logger, const ProblemJobs& pj) {
				logger("{", pj.problem_id, ", {");
				pj.jobs.for_each([&](Job j) { logger(j.id, " "); });
				logger("}}");
			};

			if (job_category.queue.size()) {
				stdlog("queue = {");
				job_category.queue.for_each([&](auto&& p) {
					auto tmplog = stdlog("   ", p.first.id, " => ");
					log_problem_job(tmplog, p.second);
					tmplog(',');
				});
				stdlog("}");
			}

			if (job_category.locked_problems.size()) {
				stdlog("locked_problems = {");
				job_category.locked_problems.for_each([&](auto&& p) {
					auto tmplog = stdlog("   ", p.first, " => ");
					log_problem_job(tmplog, p.second);
					tmplog(',');
				});
				stdlog("}");
			}
		};

		stdlog("DEBUG: judge_jobs:");
		impl(judge_jobs);
		stdlog("DEBUG: problem_jobs:");
		impl(problem_jobs);
	}

public:
	void sync_with_db() {
		STACK_UNWINDING_MARK;

		uint64_t jid;
		EnumVal<JobType> jtype;
		uint64_t aux_id;
		uint priority;
		InplaceBuff<512> info;
		// Select jobs
		auto stmt = mysql.prepare("SELECT id, type, priority, aux_id, info "
		                          "FROM jobs "
		                          "WHERE status=" JSTATUS_PENDING_STR " "
		                          "ORDER BY priority DESC, id ASC LIMIT 4");
		stmt.res_bind_all(jid, jtype, priority, aux_id, info);
		// Sets job's status to NOTICED_PENDING
		auto mark_stmt = mysql.prepare(
		   "UPDATE jobs SET status=" JSTATUS_NOTICED_PENDING_STR " WHERE id=?");
		mark_stmt.bind_all(jid);
		// Add jobs to internal queue
		using JT = JobType;
		for (stmt.execute(); stmt.next(); stmt.execute())
			do {
				DEBUG_JOB_SERVER(stdlog("DEBUG: Fetched from DB: job ", jid);)
				auto queue_job = [&jid, &priority](auto& job_category,
				                                   uint64_t problem_id,
				                                   bool locks_problem) {
					auto pinfo = job_category.problem_info.find(problem_id);
					Job curr_job {jid, priority, locks_problem};
					if (pinfo) {
						Job best_job = pinfo->second.its_best_job;
						// Get the problem's jobs (the problem may be locked)
						auto& pjobs =
						   (pinfo->second.locks_no > 0
						       ? job_category.locked_problems[problem_id]
						       : job_category.queue[best_job]);
						// Ensure field 'problem_id' is set properly (in case of
						// element creation this line is necessary)
						pjobs.problem_id = problem_id;
						// Add job to queue
						pjobs.jobs.emplace(curr_job);
						// Alter the problem's best job
						if (curr_job < best_job) {
							pinfo->second.its_best_job = curr_job;
							// Update queue (rekey ProblemJobs)
							if (pinfo->second.locks_no == 0)
								job_category.queue.alter_key(best_job,
								                             curr_job);
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
				case JT::REJUDGE_SUBMISSION:
					queue_job(judge_jobs,
					          strtoull(intentionalUnsafeStringView(
					             jobs::extractDumpedString(info))),
					          false);
					break;

				case JT::ADD_JUDGE_MODEL_SOLUTION:
					queue_job(judge_jobs, 0, false);
					break;

				case JT::REUPLOAD_JUDGE_MODEL_SOLUTION:
					queue_job(judge_jobs, aux_id, true);
					break;

					queue_job(judge_jobs, aux_id, true);
					break;

				// Problem job
				case JT::REUPLOAD_PROBLEM:
				case JT::EDIT_PROBLEM:
				case JT::DELETE_PROBLEM:
				case JT::MERGE_PROBLEMS:
				case JT::CHANGE_PROBLEM_STATEMENT:
				case JT::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
					queue_job(problem_jobs, aux_id, true);
					break;

				// Other job (local jobs that don't have associated problem)
				case JT::ADD_PROBLEM:
				case JT::CONTEST_PROBLEM_RESELECT_FINAL_SUBMISSIONS:
				case JT::DELETE_USER:
				case JT::DELETE_CONTEST:
				case JT::DELETE_CONTEST_ROUND:
				case JT::DELETE_CONTEST_PROBLEM:
				case JT::DELETE_FILE:
					other_jobs.insert({jid, priority, false});
					break;
				}
				mark_stmt.execute();
			} while (stmt.next());

		DEBUG_JOB_SERVER(
		   stdlog(__FILE__ ":", __LINE__, ": ", __FUNCTION__, "()");)
		DEBUG_JOB_SERVER(dump_queues();)
	}

	void lock_problem(uint64_t pid) {
		STACK_UNWINDING_MARK;
		DEBUG_JOB_SERVER(stdlog("DEBUG: Locking problem ", pid, "...");)

		auto lock_impl = [&pid](auto& job_category) {
			auto pinfo = job_category.problem_info.find(pid);
			if (not pinfo)
				pinfo = job_category.problem_info.insert(pid, {Job::least(), 0})
				           .first;

			if (++pinfo->second.locks_no == 1) {
				auto pj = job_category.queue.find(pinfo->second.its_best_job);
				if (pj) {
					job_category.locked_problems.insert(pid,
					                                    std::move(pj->second));
					job_category.queue.erase(pj->first);
				}
			}
		};

		lock_impl(judge_jobs);
		lock_impl(problem_jobs);

		DEBUG_JOB_SERVER(
		   stdlog(__FILE__ ":", __LINE__, ": ", __FUNCTION__, "()");)
		DEBUG_JOB_SERVER(dump_queues();)
	}

	void unlock_problem(uint64_t pid) {
		STACK_UNWINDING_MARK;
		DEBUG_JOB_SERVER(stdlog("DEBUG: Unlocking problem ", pid, "...");)

		auto unlock_impl = [&pid, this](auto& job_category) {
			auto pinfo = job_category.problem_info.find(pid);
			if (not pinfo) {
				dump_queues();
				THROW("BUG: unlocking problem that is not locked!");
			}

			if (--pinfo->second.locks_no == 0) {
				auto pl = job_category.locked_problems.find(pid);
				if (pl) {
					job_category.queue.insert(pinfo->second.its_best_job,
					                          std::move(pl->second));
					job_category.locked_problems.erase(pl->first);
				} else
					job_category.problem_info.erase(pid); /* There are no jobs
					    that belong to this problem and it is lock-free now, so
					    its records can be safely removed */
			}
		};

		unlock_impl(judge_jobs);
		unlock_impl(problem_jobs);

		DEBUG_JOB_SERVER(
		   stdlog(__FILE__ ":", __LINE__, ": ", __FUNCTION__, "()");)
		DEBUG_JOB_SERVER(dump_queues();)
	}

	class JobHolder {
		JobsQueue* jobs_queue;
		decltype(judge_jobs)* job_category;

	public:
		Job job =
		   Job::least(); // If is invalid it will be the last in comparison
		uint64_t problem_id = 0;

		JobHolder(JobsQueue& jq, decltype(judge_jobs)& jc)
		   : jobs_queue(&jq), job_category(&jc) {}

		JobHolder(JobsQueue& jq, decltype(judge_jobs)& jc, Job j, uint64_t pid)
		   : jobs_queue(&jq), job_category(&jc), job(j), problem_id(pid) {}

		JobHolder(const JobHolder&) = delete;
		JobHolder(JobHolder&&) = default;
		JobHolder& operator=(const JobHolder&) = delete;
		JobHolder& operator=(JobHolder&&) = default;

		operator Job() const noexcept { return job; }

		bool ok() const noexcept { return not(job == Job::least()); }

		// Removes the job from queue and locks problem if locks_problem is true
		void was_passed() const {
			STACK_UNWINDING_MARK;

			auto pinfo = job_category->problem_info.find(problem_id);
			if (not pinfo)
				return; // There is no such problem

			Job best_job = pinfo->second.its_best_job;
			auto& pjobs = (pinfo->second.locks_no > 0
			                  ? job_category->locked_problems[problem_id]
			                  : job_category->queue[best_job]);

			if (not pjobs.jobs.erase(job)) {
				THROW("Job erasion did not take place since the erased job was "
				      "not found");
			}

			if (pjobs.jobs.empty()) {
				// That was the last job of it's problem
				if (pinfo->second.locks_no == 0) {
					job_category->queue.erase(best_job);
					job_category->problem_info.erase(problem_id);
				} else { // Problem is locked
					job_category->locked_problems.erase(problem_id);
					pinfo->second.its_best_job = Job::least();
				}

			} else {
				// The best job of the extracted job's problem changed
				Job new_best = *pjobs.jobs.front();
				pinfo->second.its_best_job = new_best;
				// Update queue (rekey ProblemJobs)
				if (pinfo->second.locks_no == 0)
					job_category->queue.alter_key(best_job, new_best);
			}

			if (job.locks_problem)
				jobs_queue->lock_problem(problem_id);
		}
	};

private:
	JobHolder best_job(decltype(judge_jobs)& job_category) {
		if (job_category.queue.empty())
			return {*this, job_category}; // No one left

		auto it = job_category.queue.front();
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
		return best_job(problem_jobs);
	}

	class OtherJobHolder {
		decltype(other_jobs)* oj;

	public:
		Job job =
		   Job::least(); // If is invalid it will be the last in comparison

		OtherJobHolder(decltype(other_jobs)& o) : oj(&o) {}

		OtherJobHolder(decltype(other_jobs)& o, Job j) : oj(&o), job(j) {}

		OtherJobHolder(const OtherJobHolder&) = delete;
		OtherJobHolder(OtherJobHolder&&) = default;
		OtherJobHolder& operator=(const OtherJobHolder&) = delete;
		OtherJobHolder& operator=(OtherJobHolder&&) = default;

		operator Job() const noexcept { return job; }

		bool ok() const noexcept { return not(job == Job::least()); }

		// Removes the job from queue
		void was_passed() const { oj->erase(job); }
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
			THROW("eventfd() failed", errmsg());

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

	template <class Callable>
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
		bool locked_its_problem;
	};

	struct WorkerInfo {
		NextJob next_job {0, -1, false};
		std::promise<void> next_job_signalizer;
		std::future<void> next_job_waiter = next_job_signalizer.get_future();
		bool is_idle = false;
	};

private:
	mutex mtx_;
	vector<thread::id> idle_workers;
	map<thread::id, WorkerInfo>
	   workers; // AVLDict cannot be used as addresses must not change

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
			wp_.workers.emplace(tid, WorkerInfo {});
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
				     k != wp_.idle_workers.end(); ++k) {
					if (*k == tid) {
						wp_.idle_workers.erase(k);
						break;
					}
				}
			}

			if (wp_.worker_dies_callback_) {
				EventsQueue::register_event(
				   make_shared_function([& callback = wp_.worker_dies_callback_,
				                         winfo = std::move(wi)]() mutable {
					   callback(std::move(winfo));
				   }));
			}
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
	     worker_dies_callback_(std::move(dead_callback)) {
		throw_assert(job_handler_);
	}

	void spawn_worker() {
		STACK_UNWINDING_MARK;

		std::thread foo([this] {
			try {
				thread_local Worker w(*this);
				// Connect to databases
				mysql = MySQL::make_conn_with_credential_file(".db.config");

				for (;;)
					job_handler_(w.wait_for_next_job_id());

			} catch (const std::exception& e) {
				ERRLOG_CATCH(e);
				// Sleep for a while to prevent exception inundation
				std::this_thread::sleep_for(std::chrono::seconds(3));
				pthread_exit(0);
			}
		});
		foo.detach();
	}

	auto workers_no() {
		lock_guard<mutex> lock(mtx_);
		return workers.size();
	}

	bool pass_job(uint64_t job_id, int64_t problem_id = -1,
	              bool locks_problem = false) {
		STACK_UNWINDING_MARK;

		lock_guard<mutex> lock(mtx_);
		if (idle_workers.size() == 0)
			return false;

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
			if (job.locked_its_problem)
				jobs_queue.unlock_problem(job.problem_id);
		});
	};

	EnumVal<JobType> jtype;
	MySQL::Optional<uint64_t> file_id, tmp_file_id;
	MySQL::Optional<InplaceBuff<32>> creator; // TODO: use uint64_t
	InplaceBuff<32> added;
	MySQL::Optional<uint64_t> aux_id; // TODO: normalize this column...
	InplaceBuff<512> info;

	{
		auto stmt = mysql.prepare(
		   "SELECT file_id, tmp_file_id, creator, added, type, aux_id, info "
		   "FROM jobs WHERE id=? AND status!=" JSTATUS_CANCELED_STR);
		stmt.res_bind_all(file_id, tmp_file_id, creator, added, jtype, aux_id,
		                  info);
		stmt.bindAndExecute(job.id);

		if (not stmt.next()) // Job has been probably canceled
			return exit_procedures();
	}

	// Mark as in progress
	mysql.update(intentionalUnsafeStringView(
	   concat("UPDATE jobs SET status=" JSTATUS_IN_PROGRESS_STR " WHERE id=",
	          job.id)));

	stdlog("Processing job ", job.id, "...");

	Optional<StringView> creat;
	if (creator.has_value())
		creat = creator.value();
	job_dispatcher(job.id, jtype, file_id, tmp_file_id, creat, aux_id, info,
	               added);

	exit_procedures();
}

static void process_local_job(const WorkersPool::NextJob& job) {
	STACK_UNWINDING_MARK;

	stdlog(pthread_self(), " got local job {id:", job.id,
	       ", problem: ", job.problem_id, ", locked: ", job.locked_its_problem,
	       '}');

	process_job(job);
}

static void process_judge_job(const WorkersPool::NextJob& job) {
	STACK_UNWINDING_MARK;

	stdlog(pthread_self(), " got judge job {id:", job.id,
	       ", problem: ", job.problem_id, ", locked: ", job.locked_its_problem,
	       '}');

	process_job(job);
}

static void sync_and_assign_jobs();

static WorkersPool local_workers(
   process_local_job, sync_and_assign_jobs, [](WorkersPool::WorkerInfo winfo) {
	   STACK_UNWINDING_MARK;

	   // Job has to be reset and cleanup to be done
	   if (not winfo.is_idle) {
		   if (winfo.next_job.locked_its_problem)
			   jobs_queue.unlock_problem(winfo.next_job.problem_id);

		   jobs::restart_job(
		      mysql, intentionalUnsafeStringView(toStr(winfo.next_job.id)),
		      false);
	   }

	   EventsQueue::register_event([] { spawn_worker(local_workers); });
   });

static WorkersPool judge_workers(
   process_judge_job, sync_and_assign_jobs, [](WorkersPool::WorkerInfo winfo) {
	   STACK_UNWINDING_MARK;

	   // Job has to be reset and cleanup to be done
	   if (not winfo.is_idle) {
		   if (winfo.next_job.locked_its_problem)
			   jobs_queue.unlock_problem(winfo.next_job.problem_id);

		   jobs::restart_job(
		      mysql, intentionalUnsafeStringView(toStr(winfo.next_job.id)),
		      false);
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
		bool ok;
		JobsQueue::Job job;
	};

	array<SItem, 3> selector {{
	   // (is ok, best job)
	   {problem_job.ok(), problem_job},
	   {other_job.ok(), other_job},
	   {judge_job.ok(), judge_job},
	}};

	while (selector[0].ok or selector[1].ok or selector[2].ok) {
		// Select best job
		auto best_job = JobsQueue::Job::least();
		uint idx = selector.size();
		for (uint i = 0; i < selector.size(); ++i)
			if (selector[i].ok and selector[i].job < best_job) {
				idx = i;
				best_job = selector[i].job;
			}

		DEBUG_JOB_SERVER(stdlog("DEBUG: Got job: {id: ", best_job.id,
		                        ", pri: ", best_job.priority,
		                        ", locks: ", best_job.locks_problem, "}"));

		if (idx == 0) { // Problem job
			if (local_workers.pass_job(best_job.id, problem_job.problem_id,
			                           best_job.locks_problem)) {
				problem_job.was_passed();
			} else
				selector[0].ok = false; // No more workers available

		} else if (idx == 1) { // Other job
			if (local_workers.pass_job(best_job.id))
				other_job.was_passed();
			else
				selector[1].ok = false; // No more workers available

		} else if (idx == 2) { // Judge job
			if (judge_workers.pass_job(best_job.id, judge_job.problem_id,
			                           best_job.locks_problem)) {
				judge_job.was_passed();
			} else
				selector[2].ok = false; // No more workers available

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

				THROW("read() failed", errmsg());
			}

			// Update jobs queue and distribute new jobs
			sync_and_assign_jobs();

			struct inotify_event* event = (struct inotify_event*)inotify_buff;
			if (event->mask & IN_MOVE_SELF) {
				// If notify file has been moved
				(void)createFile(JOB_SERVER_NOTIFYING_FILE, S_IRUSR);
				inotify_rm_watch(inotify_fd, inotify_wd);
				inotify_wd = -1;
				return false;

			} else if (event->mask & IN_IGNORED) {
				// If notify file has disappeared
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
			sync_and_assign_jobs();

			// Inotify is broken
			if (inotify_wd == -1) {
				// Try to reset inotify
				if (inotify_fd == -1) {
					inotify_fd = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
					if (inotify_fd == -1)
						errlog("inotify_init1() failed", errmsg());
					else
						goto inotify_partially_works;

				} else {
					// Try to fix inotify_wd
				inotify_partially_works:
					// Ensure that notify-file exists
					if (access(JOB_SERVER_NOTIFYING_FILE, F_OK) == -1)
						(void)createFile(JOB_SERVER_NOTIFYING_FILE, S_IRUSR);

					inotify_wd =
					   inotify_add_watch(inotify_fd, JOB_SERVER_NOTIFYING_FILE,
					                     IN_ATTRIB | IN_MOVE_SELF);
					if (inotify_wd == -1)
						errlog("inotify_add_watch() failed", errmsg());
					else
						continue; // Fixing was successful
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

					auto tend = std::chrono::steady_clock::now() +
					            std::chrono::seconds(5);
					// Events queue is still broken, so process events for
					// approximately 1 s and try fixing again
					for (;;) {
						if (tend < std::chrono::steady_clock::now())
							break;

						jobs_queue.sync_with_db();
						while (EventsQueue::process_next_event()) {
						}

						std::this_thread::sleep_for(
						   std::chrono::milliseconds(SLEEP_INTERVAL));
					}

				} else {
					// Events queue is fine
					STACK_UNWINDING_MARK;

					pollfd pfd = {EventsQueue::get_notifier_fd(), POLLIN, 0};
					auto tend = std::chrono::steady_clock::now() +
					            std::chrono::seconds(5);
					// Process events for 5 seconds
					for (;;) {
						if (tend < std::chrono::steady_clock::now())
							break;

						// Update queue - sth may have changed
						sync_and_assign_jobs();

						int rc = poll(&pfd, 1, SLEEP_INTERVAL);
						if (rc == -1 and errno != EINTR)
							THROW("poll() failed", errmsg());

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
					auto tend = std::chrono::steady_clock::now() +
					            std::chrono::seconds(5);
					// Process events for approx 5 seconds
					for (;;) {
						if (tend < std::chrono::steady_clock::now())
							break;

						int rc = poll(&pfd, 1, SLEEP_INTERVAL);
						if (rc == -1 and errno != EINTR)
							THROW("poll() failed", errmsg());

						if (not process_inotify_event())
							continue; // inotify has just broken

						while (EventsQueue::process_next_event()) {
						}
					}

				} else {
					// Best scenario - everything works well
					STACK_UNWINDING_MARK;

					constexpr uint INFY_IDX = 0;
					constexpr uint EQ_IDX = 1;
					pollfd pfd[2] = {
					   {inotify_fd, POLLIN, 0},
					   {EventsQueue::get_notifier_fd(), POLLIN, 0}};

					for (;;) {
						int rc = poll(pfd, 2, -1);
						if (rc == -1) {
							if (errno == EINTR)
								continue;

							THROW("poll() failed", errmsg());
						}

						// This should be checked (called) first in so as to
						// update the jobs queue before processing events
						if (pfd[INFY_IDX].revents != 0)
							if (not process_inotify_event())
								continue; // inotify has just broken

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
static void cleanUpDB() {
	STACK_UNWINDING_MARK;

	try {
		// Do it in a transaction to speed things up
		auto transaction = mysql.start_transaction();

		// Fix jobs that are in progress after the job-server died
		auto res = mysql.query("SELECT id, type, info FROM jobs WHERE "
		                       "status=" JSTATUS_IN_PROGRESS_STR);
		while (res.next())
			jobs::restart_job(mysql, res[0], JobType(strtoull(res[1])), res[2],
			                  false);

		// Fix jobs that are noticed [ending] after the job-server died
		mysql.update("UPDATE jobs SET status=" JSTATUS_PENDING_STR
		             " WHERE status=" JSTATUS_NOTICED_PENDING_STR);
		transaction.commit();

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
	if (freopen(JOB_SERVER_LOG, "ae", stdout) == nullptr ||
	    dup3(STDOUT_FILENO, STDERR_FILENO, O_CLOEXEC) == -1) {
		errlog("Failed to open `", JOB_SERVER_LOG, '`', errmsg());
	}

	try {
		errlog.open(JOB_SERVER_ERROR_LOG);
	} catch (const std::exception& e) {
		errlog("Failed to open `", JOB_SERVER_ERROR_LOG, "`: ", e.what());
	}

	// Install signal handlers
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = &exit;

	(void)sigaction(SIGINT, &sa, nullptr);
	(void)sigaction(SIGQUIT, &sa, nullptr);
	(void)sigaction(SIGTERM, &sa, nullptr);

	// Connect to the databases
	try {
		mysql = MySQL::make_conn_with_credential_file(".db.config");

		cleanUpDB();

		ConfigFile cf;
		cf.add_vars("js_local_workers", "js_judge_workers");
		cf.load_config_from_file("sim.conf");

		int lworkers_no = cf["js_local_workers"].as_int();
		if (lworkers_no < 1)
			THROW("sim.conf: js_local_workers cannot be lower than 1");

		int jworkers_no = cf["js_judge_workers"].as_int();
		if (jworkers_no < 1)
			THROW("sim.conf: js_judge_workers cannot be lower than 1");

		// clang-format off
		stdlog("\n=================== Job server launched ==================="
		       "\nPID: ", getpid(),
		       "\nlocal workers: ", lworkers_no,
		       "\njudge workers: ", jworkers_no);
		// clang-format on

		for (int i = 0; i < lworkers_no; ++i)
			spawn_worker(local_workers);

		for (int i = 0; i < jworkers_no; ++i)
			spawn_worker(judge_workers);

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return 1;
	}

	eventsLoop();

	return 0;
}

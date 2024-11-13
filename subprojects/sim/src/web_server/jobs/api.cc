#include "../capabilities/jobs.hh"
#include "../http/response.hh"
#include "../web_worker/context.hh"
#include "api.hh"

#include <cstdint>
#include <optional>
#include <sim/contest_users/contest_user.hh>
#include <sim/jobs/job.hh>
#include <sim/problems/problem.hh>
#include <sim/sql/sql.hh>
#include <sim/submissions/submission.hh>
#include <sim/users/user.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/enum_to_underlying_type.hh>
#include <simlib/json_str/json_str.hh>
#include <simlib/macros/stack_unwinding.hh>
#include <simlib/throw_assert.hh>

using sim::contest_users::ContestUser;
using sim::jobs::Job;
using sim::problems::Problem;
using sim::sql::Condition;
using sim::sql::Select;
using sim::submissions::Submission;
using sim::users::User;
using std::optional;
using web_server::http::Response;
using web_server::web_worker::Context;

namespace capabilities = web_server::capabilities;

namespace {

struct JobInfo {
    decltype(Job::id) id;
    decltype(Job::created_at) created_at;
    decltype(Job::creator) creator_id;
    optional<decltype(User::username)> creator_username;
    optional<decltype(User::first_name)> creator_first_name;
    optional<decltype(User::last_name)> creator_last_name;
    decltype(Job::type) type;
    decltype(Job::priority) priority;
    decltype(Job::status) status;
    decltype(Job::aux_id) aux_id;
    decltype(Job::aux_id_2) aux_id_2;

    explicit JobInfo() = default;
    JobInfo(const JobInfo&) = delete;
    JobInfo(JobInfo&&) = delete;
    JobInfo& operator=(const JobInfo&) = delete;
    JobInfo& operator=(JobInfo&&) = delete;
    ~JobInfo() = default;

    void append_to(json_str::ObjectBuilder& obj, const capabilities::JobCapabilities& caps) {
        throw_assert(caps.view);
        obj.prop("id", id);
        obj.prop("created_at", created_at);
        if (creator_id) {
            obj.prop_obj("creator", [&](auto& obj) {
                obj.prop("id", *creator_id);
                if (creator_username) {
                    obj.prop("username", creator_username);
                    obj.prop("first_name", creator_first_name);
                    obj.prop("last_name", creator_last_name);
                } else {
                    obj.prop("username", nullptr);
                    obj.prop("first_name", nullptr);
                    obj.prop("last_name", nullptr);
                }
            });
        } else {
            obj.prop("creator", nullptr);
        }
        obj.prop("type", type);
        obj.prop("priority", priority);
        obj.prop("status", status);

        switch (type) {
        case Job::Type::JUDGE_SUBMISSION:
        case Job::Type::REJUDGE_SUBMISSION: obj.prop("submission_id", aux_id); break;
        case Job::Type::ADD_PROBLEM:
        case Job::Type::REUPLOAD_PROBLEM:
        case Job::Type::EDIT_PROBLEM:
        case Job::Type::DELETE_PROBLEM:
        case Job::Type::CHANGE_PROBLEM_STATEMENT:
        case Job::Type::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
            obj.prop("problem_id", aux_id);
            break;
        case Job::Type::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
        case Job::Type::DELETE_CONTEST_PROBLEM: obj.prop("contest_problem_id", aux_id); break;
        case Job::Type::DELETE_USER: obj.prop("user_id", aux_id); break;
        case Job::Type::DELETE_CONTEST: obj.prop("contest_id", aux_id); break;
        case Job::Type::DELETE_CONTEST_ROUND: obj.prop("contest_round_id", aux_id); break;
        case Job::Type::MERGE_PROBLEMS:
            obj.prop("donor_problem_id", aux_id);
            obj.prop("target_problem_id", aux_id_2);
            break;
        case Job::Type::DELETE_INTERNAL_FILE: obj.prop("internal_file_id", aux_id); break;
        case Job::Type::MERGE_USERS:
            obj.prop("donor_user_id", aux_id);
            obj.prop("target_user_id", aux_id_2);
            break;
        }

        obj.prop_obj("capabilities", [&](auto& obj) {
            obj.prop("view", caps.view);
            obj.prop("view_log", caps.view_log);
            obj.prop("download_attached_file", caps.download_attached_file);
            obj.prop("cancel", caps.cancel);
            obj.prop("restart", caps.restart);
            obj.prop("change_priority", caps.change_priority);
        });
    }
};

template <class... Params>
Response do_list(Context& ctx, uint32_t limit, Condition<Params...>&& where_cond) {
    STACK_UNWINDING_MARK;

    JobInfo j;
    decltype(Problem::owner_id) job_problem_owner_id;
    decltype(Problem::owner_id) job_problem_2_owner_id;
    decltype(Problem::owner_id) job_submission_problem_owner_id;
    optional<decltype(ContestUser::mode)> job_submission_contest_user_mode_of_session_user;
    auto stmt =
        ctx.mysql.execute(Select("j.id, j.created_at, j.creator, u.username, u.first_name, "
                                 "u.last_name, j.type, j.priority, j.status, j.aux_id, j.aux_id_2, "
                                 "jp.owner_id, jp2.owner_id, jsp.owner_id, jscu.mode")
                              .from("jobs j")
                              .left_join("users u")
                              .on("u.id=j.creator")
                              .left_join("problems jp")
                              .on("jp.id=j.aux_id AND j.type IN (?, ?, ?, ?, ?, ?, ?)",
                                  Job::Type::ADD_PROBLEM,
                                  Job::Type::REUPLOAD_PROBLEM,
                                  Job::Type::EDIT_PROBLEM,
                                  Job::Type::DELETE_PROBLEM,
                                  Job::Type::MERGE_PROBLEMS,
                                  Job::Type::CHANGE_PROBLEM_STATEMENT,
                                  Job::Type::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION)
                              .left_join("problems jp2")
                              .on("jp2.id=j.aux_id_2 AND j.type=?", Job::Type::MERGE_PROBLEMS)
                              .left_join("submissions js")
                              .on("js.id=j.aux_id AND j.type IN (?, ?)",
                                  Job::Type::JUDGE_SUBMISSION,
                                  Job::Type::REJUDGE_SUBMISSION)
                              .left_join("problems jsp")
                              .on("jsp.id=js.problem_id")
                              .left_join("contest_users jscu")
                              .on("jscu.contest_id=js.contest_id AND jscu.user_id=?",
                                  ctx.session ? optional{ctx.session->user_id} : std::nullopt)
                              .where(std::move(where_cond))
                              .order_by("j.id DESC")
                              .limit("?", limit));
    stmt.res_bind(
        j.id,
        j.created_at,
        j.creator_id,
        j.creator_username,
        j.creator_first_name,
        j.creator_last_name,
        j.type,
        j.priority,
        j.status,
        j.aux_id,
        j.aux_id_2,
        job_problem_owner_id,
        job_problem_2_owner_id,
        job_submission_problem_owner_id,
        job_submission_contest_user_mode_of_session_user
    );

    json_str::Object obj;
    size_t rows_num = 0;
    obj.prop_arr("list", [&](auto& arr) {
        while (stmt.next()) {
            switch (j.type) {
            case Job::Type::JUDGE_SUBMISSION:
            case Job::Type::ADD_PROBLEM:
            case Job::Type::REUPLOAD_PROBLEM:
            case Job::Type::EDIT_PROBLEM:
            case Job::Type::DELETE_PROBLEM:
            case Job::Type::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
            case Job::Type::DELETE_USER:
            case Job::Type::DELETE_CONTEST:
            case Job::Type::DELETE_CONTEST_ROUND:
            case Job::Type::DELETE_CONTEST_PROBLEM:
            case Job::Type::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
            case Job::Type::MERGE_PROBLEMS:
            case Job::Type::REJUDGE_SUBMISSION:
            case Job::Type::DELETE_INTERNAL_FILE:
            case Job::Type::CHANGE_PROBLEM_STATEMENT:
            case Job::Type::MERGE_USERS:
                (void)"You need to update the above SQL query to make it take into account the new "
                      "value";
                break;
            }
            ++rows_num;
            arr.val_obj([&](auto& obj) {
                j.append_to(
                    obj,
                    capabilities::job(
                        ctx.session,
                        j.type,
                        job_problem_owner_id,
                        job_problem_2_owner_id,
                        job_submission_problem_owner_id,
                        job_submission_contest_user_mode_of_session_user
                    )
                );
            });
        }
    });
    obj.prop("may_be_more", rows_num == limit);
    return ctx.response_json(std::move(obj).into_str());
}

constexpr bool is_query_allowed(
    capabilities::JobsListCapabilities caps, optional<decltype(Job::status)> job_status
) {
    if (!job_status) {
        return caps.query_all;
    }
    switch (*job_status) {
    case Job::Status::DONE: return caps.query_with_status_done;
    case Job::Status::PENDING: return caps.query_with_status_pending;
    case Job::Status::IN_PROGRESS: return caps.query_with_status_in_progress;
    case Job::Status::FAILED: return caps.query_with_status_failed;
    case Job::Status::CANCELLED: return caps.query_with_status_cancelled;
    }
    THROW("unexpected job status");
}

template <class... Params>
Response do_list_jobs(
    Context& ctx,
    uint32_t limit,
    optional<decltype(Job::status)> job_status,
    Condition<Params...>&& where_cond
) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_jobs(ctx.session);
    if (!is_query_allowed(caps, job_status)) {
        return ctx.response_403();
    }

    return do_list(ctx, limit, std::move(where_cond));
}

template <class... Params>
Response do_list_user_jobs(
    Context& ctx,
    uint32_t limit,
    optional<decltype(Job::status)> job_status,
    Condition<Params...>&& where_cond
) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_user_jobs(ctx.session);
    if (!is_query_allowed(caps, job_status)) {
        return ctx.response_403();
    }

    return do_list(ctx, limit, std::move(where_cond));
}

template <class... Params>
Response do_list_problem_jobs(
    Context& ctx,
    uint32_t limit,
    decltype(Problem::id) problem_id,
    optional<decltype(Job::status)> job_status,
    Condition<Params...>&& where_cond
) {
    STACK_UNWINDING_MARK;

    auto stmt = ctx.mysql.execute(Select("owner_id").from("problems").where("id=?", problem_id));
    decltype(Problem::owner_id) problem_owner_id;
    stmt.res_bind(problem_owner_id);
    (void)stmt.next(); // allow listing jobs of a deleted problem

    auto caps = capabilities::list_problem_jobs(ctx.session, problem_owner_id);
    if (!is_query_allowed(caps, job_status)) {
        return ctx.response_403();
    }

    [[maybe_unused]] auto problem_job_guard = [](decltype(Job::type) job_type) {
        switch (job_type) {
        case Job::Type::JUDGE_SUBMISSION:
        case Job::Type::REJUDGE_SUBMISSION:
        case Job::Type::ADD_PROBLEM:
        case Job::Type::REUPLOAD_PROBLEM:
        case Job::Type::EDIT_PROBLEM:
        case Job::Type::DELETE_PROBLEM:
        case Job::Type::MERGE_PROBLEMS:
        case Job::Type::CHANGE_PROBLEM_STATEMENT:
        case Job::Type::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
        case Job::Type::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
        case Job::Type::DELETE_USER:
        case Job::Type::DELETE_CONTEST:
        case Job::Type::DELETE_CONTEST_ROUND:
        case Job::Type::DELETE_CONTEST_PROBLEM:
        case Job::Type::DELETE_INTERNAL_FILE:
        case Job::Type::MERGE_USERS:
            static_assert(
                true, "On adding new entry you need to update the below Condition() statements"
            );
        }
    };
    return do_list(
        ctx,
        limit,
        (Condition(
             "j.aux_id=? AND j.type IN (?, ?, ?, ?, ?, ?, ?)",
             problem_id,
             Job::Type::ADD_PROBLEM,
             Job::Type::REUPLOAD_PROBLEM,
             Job::Type::EDIT_PROBLEM,
             Job::Type::DELETE_PROBLEM,
             Job::Type::MERGE_PROBLEMS,
             Job::Type::CHANGE_PROBLEM_STATEMENT,
             Job::Type::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION
         ) ||
         Condition("j.aux_id_2=? AND j.type=?", problem_id, Job::Type::MERGE_PROBLEMS)) &&
            std::move(where_cond)
    );
}

template <class... Params>
Response do_list_submission_jobs(
    Context& ctx,
    uint32_t limit,
    decltype(Submission::id) submission_id,
    optional<decltype(Job::status)> job_status,
    Condition<Params...>&& where_cond
) {
    STACK_UNWINDING_MARK;

    auto stmt =
        ctx.mysql.execute(Select("p.owner_id, cu.mode")
                              .from("submissions s")
                              .left_join("problems p")
                              .on("p.id=s.problem_id")
                              .left_join("contest_users cu")
                              .on("cu.contest_id=s.contest_id AND cu.user_id=?",
                                  ctx.session ? optional{ctx.session->user_id} : std::nullopt)
                              .where("s.id=?", submission_id));
    decltype(Problem::owner_id) submission_problem_owner_id;
    std::optional<decltype(ContestUser::mode)> submission_contest_user_mode_of_session_user;
    stmt.res_bind(submission_problem_owner_id, submission_contest_user_mode_of_session_user);
    (void)stmt.next(); // allow listing jobs of a deleted submission

    auto caps = capabilities::list_submission_jobs(
        ctx.session, submission_problem_owner_id, submission_contest_user_mode_of_session_user
    );
    if (!is_query_allowed(caps, job_status)) {
        return ctx.response_403();
    }

    [[maybe_unused]] auto problem_job_guard = [](decltype(Job::type) job_type) {
        switch (job_type) {
        case Job::Type::JUDGE_SUBMISSION:
        case Job::Type::REJUDGE_SUBMISSION:
        case Job::Type::ADD_PROBLEM:
        case Job::Type::REUPLOAD_PROBLEM:
        case Job::Type::EDIT_PROBLEM:
        case Job::Type::DELETE_PROBLEM:
        case Job::Type::MERGE_PROBLEMS:
        case Job::Type::CHANGE_PROBLEM_STATEMENT:
        case Job::Type::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
        case Job::Type::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
        case Job::Type::DELETE_USER:
        case Job::Type::DELETE_CONTEST:
        case Job::Type::DELETE_CONTEST_ROUND:
        case Job::Type::DELETE_CONTEST_PROBLEM:
        case Job::Type::DELETE_INTERNAL_FILE:
        case Job::Type::MERGE_USERS:
            static_assert(
                true, "On adding new entry you need to update the below Condition() statements"
            );
        }
    };
    return do_list(
        ctx,
        limit,
        Condition(
            "j.aux_id=? AND j.type IN (?, ?)",
            submission_id,
            Job::Type::JUDGE_SUBMISSION,
            Job::Type::REJUDGE_SUBMISSION
        ) && std::move(where_cond)
    );
}

} // namespace

namespace web_server::jobs::api {

constexpr inline uint32_t FIRST_QUERY_LIMIT = 64;
constexpr inline uint32_t NEXT_QUERY_LIMIT = 128;

Response list_jobs(Context& ctx) {
    STACK_UNWINDING_MARK;
    return do_list_jobs(ctx, FIRST_QUERY_LIMIT, std::nullopt, Condition("TRUE"));
}

Response list_jobs_below_id(Context& ctx, decltype(Job::id) job_id) {
    STACK_UNWINDING_MARK;
    return do_list_jobs(ctx, NEXT_QUERY_LIMIT, std::nullopt, Condition("j.id<?", job_id));
}

Response list_jobs_with_status(Context& ctx, decltype(Job::status) job_status) {
    STACK_UNWINDING_MARK;
    return do_list_jobs(ctx, FIRST_QUERY_LIMIT, job_status, Condition("j.status=?", job_status));
}

Response list_jobs_with_status_below_id(
    Context& ctx, decltype(Job::status) job_status, decltype(Job::id) job_id
) {
    STACK_UNWINDING_MARK;
    return do_list_jobs(
        ctx, NEXT_QUERY_LIMIT, job_status, Condition("j.status=? AND j.id<?", job_status, job_id)
    );
}

Response list_user_jobs(Context& ctx, decltype(User::id) user_id) {
    STACK_UNWINDING_MARK;
    return do_list_user_jobs(
        ctx, FIRST_QUERY_LIMIT, std::nullopt, Condition("j.creator=?", user_id)
    );
}

Response
list_user_jobs_below_id(Context& ctx, decltype(User::id) user_id, decltype(Job::id) job_id) {
    STACK_UNWINDING_MARK;
    return do_list_user_jobs(
        ctx, NEXT_QUERY_LIMIT, std::nullopt, Condition("j.creator=? AND j.id<?", user_id, job_id)
    );
}

Response list_user_jobs_with_status(
    Context& ctx, decltype(User::id) user_id, decltype(Job::status) job_status
) {
    STACK_UNWINDING_MARK;
    return do_list_user_jobs(
        ctx,
        FIRST_QUERY_LIMIT,
        job_status,
        Condition("j.creator=? AND j.status=?", user_id, job_status)
    );
}

Response list_user_jobs_with_status_below_id(
    Context& ctx,
    decltype(User::id) user_id,
    decltype(Job::status) job_status,
    decltype(Job::id) job_id
) {
    STACK_UNWINDING_MARK;
    return do_list_user_jobs(
        ctx,
        NEXT_QUERY_LIMIT,
        job_status,
        Condition("j.creator=? AND j.status=? AND j.id<?", user_id, job_status, job_id)
    );
}

Response list_problem_jobs(Context& ctx, decltype(Problem::id) problem_id) {
    STACK_UNWINDING_MARK;
    return do_list_problem_jobs(
        ctx, FIRST_QUERY_LIMIT, problem_id, std::nullopt, Condition("TRUE")
    );
}

Response list_problem_jobs_below_id(
    Context& ctx, decltype(Problem::id) problem_id, decltype(Job::id) job_id
) {
    STACK_UNWINDING_MARK;
    return do_list_problem_jobs(
        ctx, NEXT_QUERY_LIMIT, problem_id, std::nullopt, Condition("j.id<?", job_id)
    );
}

Response list_problem_jobs_with_status(
    Context& ctx, decltype(Problem::id) problem_id, decltype(Job::status) job_status
) {
    STACK_UNWINDING_MARK;
    return do_list_problem_jobs(
        ctx, FIRST_QUERY_LIMIT, problem_id, job_status, Condition("j.status=?", job_status)
    );
}

Response list_problem_jobs_with_status_below_id(
    Context& ctx,
    decltype(Problem::id) problem_id,
    decltype(Job::status) job_status,
    decltype(Job::id) job_id
) {
    STACK_UNWINDING_MARK;
    return do_list_problem_jobs(
        ctx,
        NEXT_QUERY_LIMIT,
        problem_id,
        job_status,
        Condition("j.status=? AND j.id<?", job_status, job_id)
    );
}

Response list_submission_jobs(Context& ctx, decltype(Submission::id) submission_id) {
    STACK_UNWINDING_MARK;
    return do_list_submission_jobs(
        ctx, FIRST_QUERY_LIMIT, submission_id, std::nullopt, Condition("TRUE")
    );
}

Response list_submission_jobs_below_id(
    Context& ctx, decltype(Submission::id) submission_id, decltype(Job::id) job_id
) {
    STACK_UNWINDING_MARK;
    return do_list_submission_jobs(
        ctx, NEXT_QUERY_LIMIT, submission_id, std::nullopt, Condition("j.id<?", job_id)
    );
}

Response list_submission_jobs_with_status(
    Context& ctx, decltype(Submission::id) submission_id, decltype(Job::status) job_status
) {
    STACK_UNWINDING_MARK;
    return do_list_submission_jobs(
        ctx, FIRST_QUERY_LIMIT, submission_id, job_status, Condition("j.status=?", job_status)
    );
}

Response list_submission_jobs_with_status_below_id(
    Context& ctx,
    decltype(Submission::id) submission_id,
    decltype(Job::status) job_status,
    decltype(Job::id) job_id
) {
    STACK_UNWINDING_MARK;
    return do_list_submission_jobs(
        ctx,
        NEXT_QUERY_LIMIT,
        submission_id,
        job_status,
        Condition("j.status=? AND j.id<?", job_status, job_id)
    );
}

Response view_job(Context& ctx, decltype(Job::id) job_id) {
    STACK_UNWINDING_MARK;

    JobInfo j;
    j.id = job_id;
    {
        auto stmt =
            ctx.mysql.execute(Select("type, aux_id, aux_id_2").from("jobs").where("id=?", job_id));
        stmt.res_bind(j.type, j.aux_id, j.aux_id_2);
        if (!stmt.next()) {
            return ctx.response_404();
        }
    }
    decltype(Problem::owner_id) job_problem_owner_id;
    decltype(Problem::owner_id) job_problem_2_owner_id;
    decltype(Problem::owner_id) job_submission_problem_owner_id;
    optional<decltype(ContestUser::mode)> job_submission_contest_user_mode_of_session_user;
    switch (j.type) {
    case Job::Type::JUDGE_SUBMISSION:
    case Job::Type::REJUDGE_SUBMISSION: {
        auto stmt =
            ctx.mysql.execute(Select("p.owner_id, cu.mode")
                                  .from("submissions s")
                                  .left_join("problems p")
                                  .on("p.id=s.problem_id")
                                  .left_join("contest_users cu")
                                  .on("cu.contest_id=s.contest_id AND cu.user_id=?",
                                      ctx.session ? optional{ctx.session->user_id} : std::nullopt)
                                  .where("s.id=?", j.aux_id));
        stmt.res_bind(
            job_submission_problem_owner_id, job_submission_contest_user_mode_of_session_user
        );
        stmt.next();
    } break;
    case Job::Type::ADD_PROBLEM:
    case Job::Type::REUPLOAD_PROBLEM:
    case Job::Type::EDIT_PROBLEM:
    case Job::Type::DELETE_PROBLEM:
    case Job::Type::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
    case Job::Type::CHANGE_PROBLEM_STATEMENT: {
        auto stmt = ctx.mysql.execute(Select("owner_id").from("problems").where("id=?", j.aux_id));
        stmt.res_bind(job_problem_owner_id);
        stmt.next();
    } break;
    case Job::Type::MERGE_PROBLEMS: {
        {
            auto stmt =
                ctx.mysql.execute(Select("owner_id").from("problems").where("id=?", j.aux_id));
            stmt.res_bind(job_problem_owner_id);
            stmt.next();
        }
        auto stmt =
            ctx.mysql.execute(Select("owner_id").from("problems").where("id=?", j.aux_id_2));
        stmt.res_bind(job_problem_2_owner_id);
        stmt.next();
    } break;
    case Job::Type::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
    case Job::Type::DELETE_USER:
    case Job::Type::DELETE_CONTEST:
    case Job::Type::DELETE_CONTEST_ROUND:
    case Job::Type::DELETE_CONTEST_PROBLEM:
    case Job::Type::DELETE_INTERNAL_FILE:
    case Job::Type::MERGE_USERS: break;
    }
    auto caps = capabilities::job(
        ctx.session,
        j.type,
        job_problem_owner_id,
        job_problem_2_owner_id,
        job_submission_problem_owner_id,
        job_submission_contest_user_mode_of_session_user
    );
    if (!caps.view) {
        return ctx.response_403();
    }
    auto stmt = ctx.mysql.execute(Select("j.created_at, j.creator, u.username, u.first_name, "
                                         "u.last_name, j.priority, j.status, j.log")
                                      .from("jobs j")
                                      .left_join("users u")
                                      .on("u.id=j.creator")
                                      .where("j.id=?", job_id));
    decltype(Job::log) job_log;
    stmt.res_bind(
        j.created_at,
        j.creator_id,
        j.creator_username,
        j.creator_first_name,
        j.creator_last_name,
        j.priority,
        j.status,
        job_log
    );
    throw_assert(stmt.next());

    json_str::Object obj;
    j.append_to(obj, caps);
    if (caps.view_log) {
        obj.prop("log", job_log);
    }
    return ctx.response_json(std::move(obj).into_str());
}

} // namespace web_server::jobs::api

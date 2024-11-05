#include "../capabilities/submissions.hh"
#include "../http/response.hh"
#include "../web_worker/context.hh"
#include "api.hh"

#include <cstdint>
#include <optional>
#include <sim/contest_problems/contest_problem.hh>
#include <sim/contest_rounds/contest_round.hh>
#include <sim/contest_users/contest_user.hh>
#include <sim/contests/contest.hh>
#include <sim/problems/problem.hh>
#include <sim/sql/sql.hh>
#include <sim/submissions/submission.hh>
#include <sim/users/user.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/json_str/json_str.hh>
#include <simlib/macros/stack_unwinding.hh>
#include <simlib/time.hh>

using sim::contest_problems::ContestProblem;
using sim::contest_rounds::ContestRound;
using sim::contest_users::ContestUser;
using sim::contests::Contest;
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

struct SubmissionInfo {
    decltype(Submission::id) id;
    decltype(Submission::created_at) created_at;
    decltype(Submission::user_id) user_id;
    std::optional<decltype(User::username)> user_username;
    std::optional<decltype(User::first_name)> user_first_name;
    std::optional<decltype(User::last_name)> user_last_name;
    decltype(Submission::problem_id) problem_id;
    decltype(Problem::name) problem_name;
    decltype(Submission::contest_problem_id) contest_problem_id;
    std::optional<decltype(ContestProblem::name)> contest_problem_name;
    decltype(Submission::contest_round_id) contest_round_id;
    std::optional<decltype(ContestRound::name)> contest_round_name;
    decltype(Submission::contest_id) contest_id;
    std::optional<decltype(Contest::name)> contest_name;
    decltype(Submission::type) type;
    decltype(Submission::language) language;
    decltype(Submission::problem_final) is_problem_final;
    decltype(Submission::contest_problem_final) is_contest_problem_final;
    decltype(Submission::contest_problem_initial_final) is_contest_problem_initial_final;
    decltype(Submission::initial_status) initial_status;
    decltype(Submission::full_status) full_status;
    decltype(Submission::score) score;

    explicit SubmissionInfo() = default;
    SubmissionInfo(const SubmissionInfo&) = delete;
    SubmissionInfo(SubmissionInfo&&) = delete;
    SubmissionInfo& operator=(const SubmissionInfo&) = delete;
    SubmissionInfo& operator=(SubmissionInfo&&) = delete;
    ~SubmissionInfo() = default;

    void append_to(json_str::ObjectBuilder& obj, const capabilities::SubmissionCapabilities& caps) {
        throw_assert(caps.view);
        obj.prop("id", id);
        obj.prop("created_at", created_at);
        if (user_id) {
            obj.prop_obj("user", [&](auto& obj) {
                obj.prop("id", user_id);
                obj.prop("username", user_username);
                obj.prop("first_name", user_first_name);
                obj.prop("last_name", user_last_name);
            });
        } else {
            obj.prop("user", nullptr);
        }
        obj.prop_obj("problem", [&](auto& obj) {
            obj.prop("id", problem_id);
            obj.prop("name", problem_name);
        });
        if (contest_problem_id) {
            obj.prop_obj("contest_problem", [&](auto& obj) {
                obj.prop("id", contest_problem_id);
                obj.prop("name", contest_problem_name);
            });
        } else {
            obj.prop("contest_problem", nullptr);
        }
        if (contest_round_id) {
            obj.prop_obj("contest_round", [&](auto& obj) {
                obj.prop("id", contest_round_id);
                obj.prop("name", contest_round_name);
            });
        } else {
            obj.prop("contest_round", nullptr);
        }
        if (contest_id) {
            obj.prop_obj("contest", [&](auto& obj) {
                obj.prop("id", contest_id);
                obj.prop("name", contest_name);
            });
        } else {
            obj.prop("contest", nullptr);
        }
        obj.prop("type", type);

        if (caps.view_if_problem_final) {
            obj.prop("is_problem_final", is_problem_final);
        } else {
            obj.prop("is_problem_final", nullptr);
        }

        if (caps.view_if_contest_problem_initial_final) {
            obj.prop("is_contest_problem_initial_final", is_contest_problem_initial_final);
        } else {
            obj.prop("is_contest_problem_initial_final", nullptr);
        }

        if (caps.view_if_contest_problem_final) {
            obj.prop("is_contest_problem_final", is_contest_problem_final);
        } else {
            obj.prop("is_contest_problem_final", nullptr);
        }

        obj.prop("language", language);

        if (caps.view_initial_status) {
            obj.prop("initial_status", initial_status);
        } else {
            obj.prop("initial_status", nullptr);
        }

        if (caps.view_full_status) {
            obj.prop("full_status", full_status);
        } else {
            obj.prop("full_status", nullptr);
        }

        if (caps.view_score) {
            obj.prop("score", score);
        } else {
            obj.prop("score", nullptr);
        }

        obj.prop_obj("capabilities", [&](auto& obj) {
            obj.prop("view", caps.view);
            obj.prop("view_initial_status", caps.view_initial_status);
            obj.prop("view_full_status", caps.view_full_status);
            obj.prop("view_initial_judgement_protocol", caps.view_initial_judgement_protocol);
            obj.prop("view_full_judgement_protocol", caps.view_full_judgement_protocol);
            obj.prop("view_score", caps.view_score);
            obj.prop("view_if_problem_final", caps.view_if_problem_final);
            obj.prop(
                "view_if_contest_problem_initial_final", caps.view_if_contest_problem_initial_final
            );
            obj.prop("view_if_contest_problem_final", caps.view_if_contest_problem_final);
            obj.prop("download", caps.download);
            obj.prop("change_type", caps.change_type);
            obj.prop("rejudge", caps.rejudge);
            obj.prop("delete", caps.delete_);
            obj.prop("view_related_jobs", caps.view_related_jobs);
        });
    }
};

template <class... Params>
Response do_list(Context& ctx, uint32_t limit, Condition<Params...>&& where_cond) {
    STACK_UNWINDING_MARK;

    SubmissionInfo s;
    decltype(Problem::visibility) problem_visibility;
    decltype(Problem::owner_id) problem_owner_id;
    std::optional<decltype(ContestProblem::method_of_choosing_final_submission)>
        contest_problem_method_of_choosing_final_submission;
    std::optional<decltype(ContestProblem::score_revealing)> contest_problem_score_revealing;
    std::optional<decltype(ContestRound::full_results)> contest_round_full_results;
    std::optional<decltype(ContestUser::mode)> session_user_contest_user_mode;
    auto stmt = ctx.mysql.execute(
        Select("s.id, s.created_at, s.user_id, u.username, u.first_name, u.last_name, "
               "s.problem_id, p.name, p.visibility, p.owner_id, s.contest_problem_id, cp.name, "
               "cp.method_of_choosing_final_submission, cp.score_revealing, s.contest_round_id, "
               "cr.name, cr.full_results, c.id, c.name, cu.mode, s.type, s.language, "
               "s.problem_final, s.contest_problem_final, s.contest_problem_initial_final, "
               "s.initial_status, s.full_status, s.score")
            .from("submissions s")
            .left_join("users u")
            .on("u.id=s.user_id")
            .left_join("problems p") // left_join is faster than inner_join
            .on("p.id=s.problem_id")
            .left_join("contest_problems cp")
            .on("cp.id=s.contest_problem_id")
            .left_join("contest_rounds cr")
            .on("cr.id=s.contest_round_id")
            .left_join("contests c")
            .on("c.id=s.contest_id")
            .left_join("contest_users cu")
            .on("cu.contest_id=s.contest_id AND cu.user_id=?",
                ctx.session ? optional{ctx.session->user_id} : std::nullopt)
            .where(std::move(where_cond))
            .order_by("s.id DESC")
            .limit("?", limit)
    );
    stmt.res_bind(
        s.id,
        s.created_at,
        s.user_id,
        s.user_username,
        s.user_first_name,
        s.user_last_name,
        s.problem_id,
        s.problem_name,
        problem_visibility,
        problem_owner_id,
        s.contest_problem_id,
        s.contest_problem_name,
        contest_problem_method_of_choosing_final_submission,
        contest_problem_score_revealing,
        s.contest_round_id,
        s.contest_round_name,
        contest_round_full_results,
        s.contest_id,
        s.contest_name,
        session_user_contest_user_mode,
        s.type,
        s.language,
        s.is_problem_final,
        s.is_contest_problem_final,
        s.is_contest_problem_initial_final,
        s.initial_status,
        s.full_status,
        s.score
    );

    const auto curr_datetime = utc_mysql_datetime();
    json_str::Object obj;
    size_t rows_num = 0;
    obj.prop_arr("list", [&](auto& arr) {
        while (stmt.next()) {
            ++rows_num;
            arr.val_obj([&](auto& obj) {
                s.append_to(
                    obj,
                    capabilities::submission(
                        ctx.session,
                        s.user_id,
                        s.type,
                        problem_visibility,
                        problem_owner_id,
                        session_user_contest_user_mode,
                        contest_round_full_results,
                        contest_problem_method_of_choosing_final_submission,
                        contest_problem_score_revealing,
                        curr_datetime
                    )
                );
            });
        }
    });
    obj.prop("may_be_more", rows_num == limit);
    return ctx.response_json(std::move(obj).into_str());
}

struct ProblemInfoForGettingCapabilities {
    decltype(Problem::visibility) problem_visibility;
    decltype(Problem::owner_id) problem_owner_id;
};

optional<ProblemInfoForGettingCapabilities>
get_problem_info_for_capabilities(Context& ctx, decltype(Problem::id) problem_id) {
    ProblemInfoForGettingCapabilities res;
    auto stmt =
        ctx.mysql.execute(Select("visibility, owner_id").from("problems").where("id=?", problem_id)
        );
    stmt.res_bind(res.problem_visibility, res.problem_owner_id);
    if (!stmt.next()) {
        return std::nullopt;
    }
    return res;
}

struct ContestInfoForGettingCapabilities {
    optional<decltype(ContestUser::mode)> session_user_contest_user_mode;
};

ContestInfoForGettingCapabilities
get_contest_info_for_capabilities(Context& ctx, decltype(Contest::id) contest_id) {
    ContestInfoForGettingCapabilities res;
    auto stmt =
        ctx.mysql.execute(Select("mode")
                              .from("contest_users")
                              .where(
                                  "contest_id=? AND user_id=?",
                                  contest_id,
                                  ctx.session ? optional{ctx.session->user_id} : std::nullopt
                              ));
    stmt.res_bind(res.session_user_contest_user_mode);
    stmt.next();
    return res;
}

struct ContestRoundInfoForGettingCapabilities {
    decltype(ContestRound::full_results) contest_round_full_results;
    optional<decltype(ContestUser::mode)> session_user_contest_user_mode;
};

optional<ContestRoundInfoForGettingCapabilities>
get_contest_round_info_for_capabilities(Context& ctx, decltype(ContestRound::id) contest_round_id) {
    ContestRoundInfoForGettingCapabilities res;
    auto stmt =
        ctx.mysql.execute(Select("cr.full_results, cu.mode")
                              .from("contest_rounds cr")
                              .left_join("contest_users cu")
                              .on("cu.user_id=? AND cu.contest_id=cr.contest_id",
                                  ctx.session ? optional{ctx.session->user_id} : std::nullopt)
                              .where("cr.id=?", contest_round_id));
    stmt.res_bind(res.contest_round_full_results, res.session_user_contest_user_mode);
    if (!stmt.next()) {
        return std::nullopt;
    }
    return res;
}

struct ContestProblemInfoForGettingCapabilities {
    decltype(ContestRound::full_results) contest_round_full_results;
    optional<decltype(ContestUser::mode)> session_user_contest_user_mode;
};

optional<ContestProblemInfoForGettingCapabilities> get_contest_problem_info_for_capabilities(
    Context& ctx, decltype(ContestProblem::id) contest_problem_id
) {
    ContestProblemInfoForGettingCapabilities res;
    auto stmt =
        ctx.mysql.execute(Select("cr.full_results, cu.mode")
                              .from("contest_problems cp")
                              .left_join("contest_rounds cr") // left_join is faster than inner_join
                              .on("cr.id=cp.contest_round_id")
                              .left_join("contest_users cu")
                              .on("cu.user_id=? AND cu.contest_id=cp.contest_id",
                                  ctx.session ? optional{ctx.session->user_id} : std::nullopt)
                              .where("cp.id=?", contest_problem_id));
    stmt.res_bind(res.contest_round_full_results, res.session_user_contest_user_mode);
    if (!stmt.next()) {
        return std::nullopt;
    }
    return res;
}

} // namespace

namespace web_server::submissions::api {

constexpr inline uint32_t FIRST_QUERY_LIMIT = 64;
constexpr inline uint32_t NEXT_QUERY_LIMIT = 128;

Response list_submissions(Context& ctx) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_submissions(ctx.session);
    if (!caps.query_all) {
        return ctx.response_403();
    }
    return do_list(ctx, FIRST_QUERY_LIMIT, Condition("TRUE"));
}

Response list_submissions_below_id(Context& ctx, decltype(Submission::id) submission_id) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_submissions(ctx.session);
    if (!caps.query_all) {
        return ctx.response_403();
    }
    return do_list(ctx, NEXT_QUERY_LIMIT, Condition("s.id<?", submission_id));
}

Response list_submissions_with_type_contest_problem_final(Context& ctx) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_submissions(ctx.session);
    if (!caps.query_with_type_contest_problem_final) {
        return ctx.response_403();
    }
    return do_list(ctx, FIRST_QUERY_LIMIT, Condition("s.contest_problem_final=1"));
}

Response list_submissions_with_type_contest_problem_final_below_id(
    Context& ctx, decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_submissions(ctx.session);
    if (!caps.query_with_type_contest_problem_final) {
        return ctx.response_403();
    }
    return do_list(
        ctx, NEXT_QUERY_LIMIT, Condition("s.contest_problem_final=1 AND s.id<?", submission_id)
    );
}

Response list_submissions_with_type_problem_final(Context& ctx) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_submissions(ctx.session);
    if (!caps.query_with_type_problem_final) {
        return ctx.response_403();
    }
    return do_list(ctx, FIRST_QUERY_LIMIT, Condition("s.problem_final=1"));
}

Response list_submissions_with_type_problem_final_below_id(
    Context& ctx, decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_submissions(ctx.session);
    if (!caps.query_with_type_problem_final) {
        return ctx.response_403();
    }
    return do_list(ctx, NEXT_QUERY_LIMIT, Condition("s.problem_final=1 AND s.id<?", submission_id));
}

Response list_submissions_with_type_final(Context& ctx) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_submissions(ctx.session);
    if (!caps.query_with_type_final) {
        return ctx.response_403();
    }
    return do_list(
        ctx, FIRST_QUERY_LIMIT, Condition("s.contest_problem_final=1 OR s.problem_final=1")
    );
}

Response
list_submissions_with_type_final_below_id(Context& ctx, decltype(Submission::id) submission_id) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_submissions(ctx.session);
    if (!caps.query_with_type_final) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition("(s.contest_problem_final=1 OR s.problem_final=1) AND s.id<?", submission_id)
    );
}

Response list_submissions_with_type_ignored(Context& ctx) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_submissions(ctx.session);
    if (!caps.query_with_type_ignored) {
        return ctx.response_403();
    }
    return do_list(ctx, FIRST_QUERY_LIMIT, Condition("s.type=?", Submission::Type::IGNORED));
}

Response
list_submissions_with_type_ignored_below_id(Context& ctx, decltype(Submission::id) submission_id) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_submissions(ctx.session);
    if (!caps.query_with_type_ignored) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition("s.type=? AND s.id<?", Submission::Type::IGNORED, submission_id)
    );
}

Response list_submissions_with_type_problem_solution(Context& ctx) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_submissions(ctx.session);
    if (!caps.query_with_type_problem_solution) {
        return ctx.response_403();
    }
    return do_list(
        ctx, FIRST_QUERY_LIMIT, Condition("s.type=?", Submission::Type::PROBLEM_SOLUTION)
    );
}

Response list_submissions_with_type_problem_solution_below_id(
    Context& ctx, decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_submissions(ctx.session);
    if (!caps.query_with_type_problem_solution) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition("s.type=? AND s.id<?", Submission::Type::PROBLEM_SOLUTION, submission_id)
    );
}

Response list_user_submissions(Context& ctx, decltype(User::id) user_id) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_user_submissions(ctx.session, user_id);
    if (!caps.query_all) {
        return ctx.response_403();
    }
    return do_list(ctx, FIRST_QUERY_LIMIT, Condition("s.user_id=?", user_id));
}

Response list_user_submissions_below_id(
    Context& ctx, decltype(User::id) user_id, decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_user_submissions(ctx.session, user_id);
    if (!caps.query_all) {
        return ctx.response_403();
    }
    return do_list(
        ctx, NEXT_QUERY_LIMIT, Condition("s.user_id=? AND s.id<?", user_id, submission_id)
    );
}

Response
list_user_submissions_with_type_contest_problem_final(Context& ctx, decltype(User::id) user_id) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_user_submissions(ctx.session, user_id);
    if (!caps.query_with_type_contest_problem_final) {
        return ctx.response_403();
    }
    return do_list(
        ctx, FIRST_QUERY_LIMIT, Condition("s.user_id=? AND s.contest_problem_final=1", user_id)
    );
}

Response list_user_submissions_with_type_contest_problem_final_below_id(
    Context& ctx, decltype(User::id) user_id, decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_user_submissions(ctx.session, user_id);
    if (!caps.query_with_type_contest_problem_final) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition("s.user_id=? AND s.contest_problem_final=1 AND s.id<?", user_id, submission_id)
    );
}

Response list_user_submissions_with_type_problem_final(Context& ctx, decltype(User::id) user_id) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_user_submissions(ctx.session, user_id);
    if (!caps.query_with_type_problem_final) {
        return ctx.response_403();
    }
    return do_list(ctx, FIRST_QUERY_LIMIT, Condition("s.user_id=? AND s.problem_final=1", user_id));
}

Response list_user_submissions_with_type_problem_final_below_id(
    Context& ctx, decltype(User::id) user_id, decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_user_submissions(ctx.session, user_id);
    if (!caps.query_with_type_problem_final) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition("s.user_id=? AND s.problem_final=1 AND s.id<?", user_id, submission_id)
    );
}

Response list_user_submissions_with_type_final(Context& ctx, decltype(User::id) user_id) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_user_submissions(ctx.session, user_id);
    if (!caps.query_with_type_final) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        FIRST_QUERY_LIMIT,
        Condition("s.user_id=? AND (s.contest_problem_final=1 OR s.problem_final=1)", user_id)
    );
}

Response list_user_submissions_with_type_final_below_id(
    Context& ctx, decltype(User::id) user_id, decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_user_submissions(ctx.session, user_id);
    if (!caps.query_with_type_final) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition(
            "s.user_id=? AND (s.contest_problem_final=1 OR s.problem_final=1) AND s.id<?",
            user_id,
            submission_id
        )
    );
}

Response list_user_submissions_with_type_ignored(Context& ctx, decltype(User::id) user_id) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_user_submissions(ctx.session, user_id);
    if (!caps.query_with_type_ignored) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        FIRST_QUERY_LIMIT,
        Condition("s.user_id=? AND s.type=?", user_id, Submission::Type::IGNORED)
    );
}

Response list_user_submissions_with_type_ignored_below_id(
    Context& ctx, decltype(User::id) user_id, decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_user_submissions(ctx.session, user_id);
    if (!caps.query_with_type_ignored) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition(
            "s.user_id=? AND s.type=? AND s.id<?", user_id, Submission::Type::IGNORED, submission_id
        )
    );
}

Response list_problem_submissions(Context& ctx, decltype(Problem::id) problem_id) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_problem_submissions(ctx.session, problem_id);
    if (!caps.query_all) {
        return ctx.response_403();
    }
    return do_list(ctx, FIRST_QUERY_LIMIT, Condition("s.problem_id=?", problem_id));
}

Response list_problem_submissions_below_id(
    Context& ctx, decltype(Problem::id) problem_id, decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_problem_submissions(ctx.session, problem_id);
    if (!caps.query_all) {
        return ctx.response_403();
    }
    return do_list(
        ctx, NEXT_QUERY_LIMIT, Condition("s.problem_id=? AND s.id<?", problem_id, submission_id)
    );
}

Response list_problem_submissions_with_type_contest_problem_final(
    Context& ctx, decltype(Problem::id) problem_id
) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_problem_submissions(ctx.session, problem_id);
    if (!caps.query_with_type_contest_problem_final) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        FIRST_QUERY_LIMIT,
        Condition("s.problem_id=? AND s.contest_problem_final=1", problem_id)
    );
}

Response list_problem_submissions_with_type_contest_problem_final_below_id(
    Context& ctx, decltype(Problem::id) problem_id, decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_problem_submissions(ctx.session, problem_id);
    if (!caps.query_with_type_contest_problem_final) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition(
            "s.problem_id=? AND s.contest_problem_final=1 AND s.id<?", problem_id, submission_id
        )
    );
}

Response
list_problem_submissions_with_type_problem_final(Context& ctx, decltype(Problem::id) problem_id) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_problem_submissions(ctx.session, problem_id);
    if (!caps.query_with_type_problem_final) {
        return ctx.response_403();
    }
    return do_list(
        ctx, FIRST_QUERY_LIMIT, Condition("s.problem_id=? AND s.problem_final=1", problem_id)
    );
}

Response list_problem_submissions_with_type_problem_final_below_id(
    Context& ctx, decltype(Problem::id) problem_id, decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_problem_submissions(ctx.session, problem_id);
    if (!caps.query_with_type_problem_final) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition("s.problem_id=? AND s.problem_final=1 AND s.id<?", problem_id, submission_id)
    );
}

Response list_problem_submissions_with_type_final(Context& ctx, decltype(Problem::id) problem_id) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_problem_submissions(ctx.session, problem_id);
    if (!caps.query_with_type_final) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        FIRST_QUERY_LIMIT,
        Condition("s.problem_id=? AND (s.contest_problem_final=1 OR s.problem_final=1)", problem_id)
    );
}

Response list_problem_submissions_with_type_final_below_id(
    Context& ctx, decltype(Problem::id) problem_id, decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_problem_submissions(ctx.session, problem_id);
    if (!caps.query_with_type_final) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition(
            "s.problem_id=? AND (s.contest_problem_final=1 OR s.problem_final=1) AND s.id<?",
            problem_id,
            submission_id
        )
    );
}

Response
list_problem_submissions_with_type_ignored(Context& ctx, decltype(Problem::id) problem_id) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_problem_submissions(ctx.session, problem_id);
    if (!caps.query_with_type_ignored) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        FIRST_QUERY_LIMIT,
        Condition("s.problem_id=? AND s.type=?", problem_id, Submission::Type::IGNORED)
    );
}

Response list_problem_submissions_with_type_ignored_below_id(
    Context& ctx, decltype(Problem::id) problem_id, decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_problem_submissions(ctx.session, problem_id);
    if (!caps.query_with_type_ignored) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition(
            "s.problem_id=? AND s.type=? AND s.id<?",
            problem_id,
            Submission::Type::IGNORED,
            submission_id
        )
    );
}

Response list_problem_submissions_with_type_problem_solution(
    Context& ctx, decltype(Problem::id) problem_id
) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_problem_submissions(ctx.session, problem_id);
    if (!caps.query_with_type_problem_solution) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        FIRST_QUERY_LIMIT,
        Condition("s.problem_id=? AND s.type=?", problem_id, Submission::Type::PROBLEM_SOLUTION)
    );
}

Response list_problem_submissions_with_type_problem_solution_below_id(
    Context& ctx, decltype(Problem::id) problem_id, decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_problem_submissions(ctx.session, problem_id);
    if (!caps.query_with_type_problem_solution) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition(
            "s.problem_id=? AND s.type=? AND s.id<?",
            problem_id,
            Submission::Type::PROBLEM_SOLUTION,
            submission_id
        )
    );
}

Response list_problem_and_user_submissions(
    Context& ctx, decltype(Problem::id) problem_id, decltype(User::id) user_id
) {
    STACK_UNWINDING_MARK;

    auto problem_info_for_caps = get_problem_info_for_capabilities(ctx, problem_id);
    if (!problem_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_problem_and_user_submissions(
        ctx.session,
        problem_info_for_caps->problem_visibility,
        problem_info_for_caps->problem_owner_id,
        user_id
    );
    if (!caps.query_all) {
        return ctx.response_403();
    }
    return do_list(
        ctx, FIRST_QUERY_LIMIT, Condition("s.problem_id=? AND s.user_id=?", problem_id, user_id)
    );
}

Response list_problem_and_user_submissions_below_id(
    Context& ctx,
    decltype(Problem::id) problem_id,
    decltype(User::id) user_id,
    decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto problem_info_for_caps = get_problem_info_for_capabilities(ctx, problem_id);
    if (!problem_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_problem_and_user_submissions(
        ctx.session,
        problem_info_for_caps->problem_visibility,
        problem_info_for_caps->problem_owner_id,
        user_id
    );
    if (!caps.query_all) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition("s.problem_id=? AND s.user_id=? AND s.id<?", problem_id, user_id, submission_id)
    );
}

Response list_problem_and_user_submissions_with_type_contest_problem_final(
    Context& ctx, decltype(Problem::id) problem_id, decltype(User::id) user_id
) {
    STACK_UNWINDING_MARK;

    auto problem_info_for_caps = get_problem_info_for_capabilities(ctx, problem_id);
    if (!problem_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_problem_and_user_submissions(
        ctx.session,
        problem_info_for_caps->problem_visibility,
        problem_info_for_caps->problem_owner_id,
        user_id
    );
    if (!caps.query_with_type_contest_problem_final) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        FIRST_QUERY_LIMIT,
        Condition(
            "s.problem_id=? AND s.user_id=? AND s.contest_problem_final=1", problem_id, user_id
        )
    );
}

Response list_problem_and_user_submissions_with_type_contest_problem_final_below_id(
    Context& ctx,
    decltype(Problem::id) problem_id,
    decltype(User::id) user_id,
    decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto problem_info_for_caps = get_problem_info_for_capabilities(ctx, problem_id);
    if (!problem_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_problem_and_user_submissions(
        ctx.session,
        problem_info_for_caps->problem_visibility,
        problem_info_for_caps->problem_owner_id,
        user_id
    );
    if (!caps.query_with_type_contest_problem_final) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition(
            "s.problem_id=? AND s.user_id=? AND s.contest_problem_final=1 AND s.id<?",
            problem_id,
            user_id,
            submission_id
        )
    );
}

Response list_problem_and_user_submissions_with_type_problem_final(
    Context& ctx, decltype(Problem::id) problem_id, decltype(User::id) user_id
) {
    STACK_UNWINDING_MARK;

    auto problem_info_for_caps = get_problem_info_for_capabilities(ctx, problem_id);
    if (!problem_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_problem_and_user_submissions(
        ctx.session,
        problem_info_for_caps->problem_visibility,
        problem_info_for_caps->problem_owner_id,
        user_id
    );
    if (!caps.query_with_type_problem_final) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        FIRST_QUERY_LIMIT,
        Condition("s.problem_id=? AND s.user_id=? AND s.problem_final=1", problem_id, user_id)
    );
}

Response list_problem_and_user_submissions_with_type_problem_final_below_id(
    Context& ctx,
    decltype(Problem::id) problem_id,
    decltype(User::id) user_id,
    decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto problem_info_for_caps = get_problem_info_for_capabilities(ctx, problem_id);
    if (!problem_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_problem_and_user_submissions(
        ctx.session,
        problem_info_for_caps->problem_visibility,
        problem_info_for_caps->problem_owner_id,
        user_id
    );
    if (!caps.query_with_type_problem_final) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition(
            "s.problem_id=? AND s.user_id=? AND s.problem_final=1 AND s.id<?",
            problem_id,
            user_id,
            submission_id
        )
    );
}

Response list_problem_and_user_submissions_with_type_final(
    Context& ctx, decltype(Problem::id) problem_id, decltype(User::id) user_id
) {
    STACK_UNWINDING_MARK;

    auto problem_info_for_caps = get_problem_info_for_capabilities(ctx, problem_id);
    if (!problem_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_problem_and_user_submissions(
        ctx.session,
        problem_info_for_caps->problem_visibility,
        problem_info_for_caps->problem_owner_id,
        user_id
    );
    if (!caps.query_with_type_final) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        FIRST_QUERY_LIMIT,
        Condition(
            "s.problem_id=? AND s.user_id=? AND (s.contest_problem_final=1 OR s.problem_final=1)",
            problem_id,
            user_id
        )
    );
}

Response list_problem_and_user_submissions_with_type_final_below_id(
    Context& ctx,
    decltype(Problem::id) problem_id,
    decltype(User::id) user_id,
    decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto problem_info_for_caps = get_problem_info_for_capabilities(ctx, problem_id);
    if (!problem_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_problem_and_user_submissions(
        ctx.session,
        problem_info_for_caps->problem_visibility,
        problem_info_for_caps->problem_owner_id,
        user_id
    );
    if (!caps.query_with_type_final) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition(
            "s.problem_id=? AND s.user_id=? AND (s.contest_problem_final=1 OR s.problem_final=1)"
            " AND s.id<?",
            problem_id,
            user_id,
            submission_id
        )
    );
}

Response list_problem_and_user_submissions_with_type_ignored(
    Context& ctx, decltype(Problem::id) problem_id, decltype(User::id) user_id
) {
    STACK_UNWINDING_MARK;

    auto problem_info_for_caps = get_problem_info_for_capabilities(ctx, problem_id);
    if (!problem_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_problem_and_user_submissions(
        ctx.session,
        problem_info_for_caps->problem_visibility,
        problem_info_for_caps->problem_owner_id,
        user_id
    );
    if (!caps.query_with_type_ignored) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        FIRST_QUERY_LIMIT,
        Condition(
            "s.problem_id=? AND s.user_id=? AND s.type=?",
            problem_id,
            user_id,
            Submission::Type::IGNORED
        )
    );
}

Response list_problem_and_user_submissions_with_type_ignored_below_id(
    Context& ctx,
    decltype(Problem::id) problem_id,
    decltype(User::id) user_id,
    decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto problem_info_for_caps = get_problem_info_for_capabilities(ctx, problem_id);
    if (!problem_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_problem_and_user_submissions(
        ctx.session,
        problem_info_for_caps->problem_visibility,
        problem_info_for_caps->problem_owner_id,
        user_id
    );
    if (!caps.query_with_type_ignored) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition(
            "s.problem_id=? AND s.user_id=? AND s.type=? AND s.id<?",
            problem_id,
            user_id,
            Submission::Type::IGNORED,
            submission_id
        )
    );
}

Response list_contest_submissions(Context& ctx, decltype(Contest::id) contest_id) {
    STACK_UNWINDING_MARK;

    auto contest_info_for_caps = get_contest_info_for_capabilities(ctx, contest_id);
    auto caps = capabilities::list_contest_submissions(
        ctx.session, contest_info_for_caps.session_user_contest_user_mode
    );
    if (!caps.query_all) {
        return ctx.response_403();
    }
    return do_list(ctx, FIRST_QUERY_LIMIT, Condition("s.contest_id=?", contest_id));
}

Response list_contest_submissions_below_id(
    Context& ctx, decltype(Contest::id) contest_id, decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto contest_info_for_caps = get_contest_info_for_capabilities(ctx, contest_id);
    auto caps = capabilities::list_contest_submissions(
        ctx.session, contest_info_for_caps.session_user_contest_user_mode
    );
    if (!caps.query_all) {
        return ctx.response_403();
    }
    return do_list(
        ctx, NEXT_QUERY_LIMIT, Condition("s.contest_id=? AND s.id<?", contest_id, submission_id)
    );
}

Response list_contest_submissions_with_type_contest_problem_final(
    Context& ctx, decltype(Contest::id) contest_id
) {
    STACK_UNWINDING_MARK;

    auto contest_info_for_caps = get_contest_info_for_capabilities(ctx, contest_id);
    auto caps = capabilities::list_contest_submissions(
        ctx.session, contest_info_for_caps.session_user_contest_user_mode
    );
    if (!caps.query_with_type_contest_problem_final) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        FIRST_QUERY_LIMIT,
        Condition("s.contest_id=? AND s.contest_problem_final=1", contest_id)
    );
}

Response list_contest_submissions_with_type_contest_problem_final_below_id(
    Context& ctx, decltype(Contest::id) contest_id, decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto contest_info_for_caps = get_contest_info_for_capabilities(ctx, contest_id);
    auto caps = capabilities::list_contest_submissions(
        ctx.session, contest_info_for_caps.session_user_contest_user_mode
    );
    if (!caps.query_with_type_contest_problem_final) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition(
            "s.contest_id=? AND s.contest_problem_final=1 AND s.id<?", contest_id, submission_id
        )
    );
}

Response
list_contest_submissions_with_type_ignored(Context& ctx, decltype(Contest::id) contest_id) {
    STACK_UNWINDING_MARK;

    auto contest_info_for_caps = get_contest_info_for_capabilities(ctx, contest_id);
    auto caps = capabilities::list_contest_submissions(
        ctx.session, contest_info_for_caps.session_user_contest_user_mode
    );
    if (!caps.query_with_type_ignored) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        FIRST_QUERY_LIMIT,
        Condition("s.contest_id=? AND s.type=?", contest_id, Submission::Type::IGNORED)
    );
}

Response list_contest_submissions_with_type_ignored_below_id(
    Context& ctx, decltype(Contest::id) contest_id, decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto contest_info_for_caps = get_contest_info_for_capabilities(ctx, contest_id);
    auto caps = capabilities::list_contest_submissions(
        ctx.session, contest_info_for_caps.session_user_contest_user_mode
    );
    if (!caps.query_with_type_ignored) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition(
            "s.contest_id=? AND s.type=? AND s.id<?",
            contest_id,
            Submission::Type::IGNORED,
            submission_id
        )
    );
}

Response list_contest_and_user_submissions(
    Context& ctx, decltype(Contest::id) contest_id, decltype(User::id) user_id
) {
    STACK_UNWINDING_MARK;

    auto contest_info_for_caps = get_contest_info_for_capabilities(ctx, contest_id);
    auto caps = capabilities::list_contest_and_user_submissions(
        ctx.session, contest_info_for_caps.session_user_contest_user_mode, user_id
    );
    if (!caps.query_all) {
        return ctx.response_403();
    }
    return do_list(
        ctx, FIRST_QUERY_LIMIT, Condition("s.contest_id=? AND s.user_id=?", contest_id, user_id)
    );
}

Response list_contest_and_user_submissions_below_id(
    Context& ctx,
    decltype(Contest::id) contest_id,
    decltype(User::id) user_id,
    decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto contest_info_for_caps = get_contest_info_for_capabilities(ctx, contest_id);
    auto caps = capabilities::list_contest_and_user_submissions(
        ctx.session, contest_info_for_caps.session_user_contest_user_mode, user_id
    );
    if (!caps.query_all) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition("s.contest_id=? AND s.user_id=? AND s.id<?", contest_id, user_id, submission_id)
    );
}

Response list_contest_and_user_submissions_with_type_contest_problem_final(
    Context& ctx, decltype(Contest::id) contest_id, decltype(User::id) user_id
) {
    STACK_UNWINDING_MARK;

    auto contest_info_for_caps = get_contest_info_for_capabilities(ctx, contest_id);
    auto caps = capabilities::list_contest_and_user_submissions(
        ctx.session, contest_info_for_caps.session_user_contest_user_mode, user_id
    );
    if (!caps.query_with_type_contest_problem_final) {
        return ctx.response_403();
    }
    if (caps.query_with_type_contest_problem_final_always_show_full_results) {
        return do_list(
            ctx,
            FIRST_QUERY_LIMIT,
            Condition(
                "s.contest_id=? AND s.user_id=? AND s.contest_problem_final=1", contest_id, user_id
            )
        );
    }
    return do_list(
        ctx,
        FIRST_QUERY_LIMIT,
        Condition(
            "s.contest_id=? AND s.user_id=? AND"
            // The below condition is redundant but makes the query execute faster.
            " (s.contest_problem_final=1 OR s.contest_problem_initial_final=1) AND"
            " IF(?>=cr.full_results, s.contest_problem_final=1, s.contest_problem_initial_final=1)",
            contest_id,
            user_id,
            utc_mysql_datetime()
        )
    );
}

Response list_contest_and_user_submissions_with_type_contest_problem_final_below_id(
    Context& ctx,
    decltype(Contest::id) contest_id,
    decltype(User::id) user_id,
    decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto contest_info_for_caps = get_contest_info_for_capabilities(ctx, contest_id);
    auto caps = capabilities::list_contest_and_user_submissions(
        ctx.session, contest_info_for_caps.session_user_contest_user_mode, user_id
    );
    if (!caps.query_with_type_contest_problem_final) {
        return ctx.response_403();
    }
    if (caps.query_with_type_contest_problem_final_always_show_full_results) {
        return do_list(
            ctx,
            NEXT_QUERY_LIMIT,
            Condition(
                "s.contest_id=? AND s.user_id=? AND s.contest_problem_final=1 AND s.id<?",
                contest_id,
                user_id,
                submission_id
            )
        );
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition(
            "s.contest_id=? AND s.user_id=? AND"
            // The below condition is redundant but makes the query execute faster.
            " (s.contest_problem_final=1 OR s.contest_problem_initial_final=1) AND"
            " IF(?>=cr.full_results, s.contest_problem_final=1, s.contest_problem_initial_final=1)"
            " AND s.id<?",
            contest_id,
            user_id,
            utc_mysql_datetime(),
            submission_id
        )
    );
}

Response list_contest_and_user_submissions_with_type_ignored(
    Context& ctx, decltype(Contest::id) contest_id, decltype(User::id) user_id
) {
    STACK_UNWINDING_MARK;

    auto contest_info_for_caps = get_contest_info_for_capabilities(ctx, contest_id);
    auto caps = capabilities::list_contest_and_user_submissions(
        ctx.session, contest_info_for_caps.session_user_contest_user_mode, user_id
    );
    if (!caps.query_with_type_ignored) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        FIRST_QUERY_LIMIT,
        Condition(
            "s.contest_id=? AND s.user_id=? AND s.type=?",
            contest_id,
            user_id,
            Submission::Type::IGNORED
        )
    );
}

Response list_contest_and_user_submissions_with_type_ignored_below_id(
    Context& ctx,
    decltype(Contest::id) contest_id,
    decltype(User::id) user_id,
    decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto contest_info_for_caps = get_contest_info_for_capabilities(ctx, contest_id);
    auto caps = capabilities::list_contest_and_user_submissions(
        ctx.session, contest_info_for_caps.session_user_contest_user_mode, user_id
    );
    if (!caps.query_with_type_ignored) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition(
            "s.contest_id=? AND s.user_id=? AND s.type=? AND s.id<?",
            contest_id,
            user_id,
            Submission::Type::IGNORED,
            submission_id
        )
    );
}

Response list_contest_round_submissions(Context& ctx, decltype(ContestRound::id) contest_round_id) {
    STACK_UNWINDING_MARK;

    auto contest_round_info_for_caps =
        get_contest_round_info_for_capabilities(ctx, contest_round_id);
    if (!contest_round_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_contest_round_submissions(
        ctx.session, contest_round_info_for_caps->session_user_contest_user_mode
    );
    if (!caps.query_all) {
        return ctx.response_403();
    }
    return do_list(ctx, FIRST_QUERY_LIMIT, Condition("s.contest_round_id=?", contest_round_id));
}

Response list_contest_round_submissions_below_id(
    Context& ctx,
    decltype(ContestRound::id) contest_round_id,
    decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto contest_round_info_for_caps =
        get_contest_round_info_for_capabilities(ctx, contest_round_id);
    if (!contest_round_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_contest_round_submissions(
        ctx.session, contest_round_info_for_caps->session_user_contest_user_mode
    );
    if (!caps.query_all) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition("s.contest_round_id=? AND s.id<?", contest_round_id, submission_id)
    );
}

Response list_contest_round_submissions_with_type_contest_problem_final(
    Context& ctx, decltype(ContestRound::id) contest_round_id
) {
    STACK_UNWINDING_MARK;

    auto contest_round_info_for_caps =
        get_contest_round_info_for_capabilities(ctx, contest_round_id);
    if (!contest_round_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_contest_round_submissions(
        ctx.session, contest_round_info_for_caps->session_user_contest_user_mode
    );
    if (!caps.query_with_type_contest_problem_final) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        FIRST_QUERY_LIMIT,
        Condition("s.contest_round_id=? AND s.contest_problem_final=1", contest_round_id)
    );
}

Response list_contest_round_submissions_with_type_contest_problem_final_below_id(
    Context& ctx,
    decltype(ContestRound::id) contest_round_id,
    decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto contest_round_info_for_caps =
        get_contest_round_info_for_capabilities(ctx, contest_round_id);
    if (!contest_round_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_contest_round_submissions(
        ctx.session, contest_round_info_for_caps->session_user_contest_user_mode
    );
    if (!caps.query_with_type_contest_problem_final) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition(
            "s.contest_round_id=? AND s.contest_problem_final=1 AND s.id<?",
            contest_round_id,
            submission_id
        )
    );
}

Response list_contest_round_submissions_with_type_ignored(
    Context& ctx, decltype(ContestRound::id) contest_round_id
) {
    STACK_UNWINDING_MARK;

    auto contest_round_info_for_caps =
        get_contest_round_info_for_capabilities(ctx, contest_round_id);
    if (!contest_round_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_contest_round_submissions(
        ctx.session, contest_round_info_for_caps->session_user_contest_user_mode
    );
    if (!caps.query_with_type_ignored) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        FIRST_QUERY_LIMIT,
        Condition("s.contest_round_id=? AND s.type=?", contest_round_id, Submission::Type::IGNORED)
    );
}

Response list_contest_round_submissions_with_type_ignored_below_id(
    Context& ctx,
    decltype(ContestRound::id) contest_round_id,
    decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto contest_round_info_for_caps =
        get_contest_round_info_for_capabilities(ctx, contest_round_id);
    if (!contest_round_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_contest_round_submissions(
        ctx.session, contest_round_info_for_caps->session_user_contest_user_mode
    );
    if (!caps.query_with_type_ignored) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition(
            "s.contest_round_id=? AND s.type=? AND s.id<?",
            contest_round_id,
            Submission::Type::IGNORED,
            submission_id
        )
    );
}

Response list_contest_round_and_user_submissions(
    Context& ctx, decltype(ContestRound::id) contest_round_id, decltype(User::id) user_id
) {
    STACK_UNWINDING_MARK;

    auto contest_round_info_for_caps =
        get_contest_round_info_for_capabilities(ctx, contest_round_id);
    if (!contest_round_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_contest_round_and_user_submissions(
        ctx.session,
        contest_round_info_for_caps->session_user_contest_user_mode,
        user_id,
        contest_round_info_for_caps->contest_round_full_results,
        utc_mysql_datetime()
    );
    if (!caps.query_all) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        FIRST_QUERY_LIMIT,
        Condition("s.contest_round_id=? AND s.user_id=?", contest_round_id, user_id)
    );
}

Response list_contest_round_and_user_submissions_below_id(
    Context& ctx,
    decltype(ContestRound::id) contest_round_id,
    decltype(User::id) user_id,
    decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto contest_round_info_for_caps =
        get_contest_round_info_for_capabilities(ctx, contest_round_id);
    if (!contest_round_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_contest_round_and_user_submissions(
        ctx.session,
        contest_round_info_for_caps->session_user_contest_user_mode,
        user_id,
        contest_round_info_for_caps->contest_round_full_results,
        utc_mysql_datetime()
    );
    if (!caps.query_all) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition(
            "s.contest_round_id=? AND s.user_id=? AND s.id<?",
            contest_round_id,
            user_id,
            submission_id
        )
    );
}

Response list_contest_round_and_user_submissions_with_type_contest_problem_final(
    Context& ctx, decltype(ContestRound::id) contest_round_id, decltype(User::id) user_id
) {
    STACK_UNWINDING_MARK;

    auto contest_round_info_for_caps =
        get_contest_round_info_for_capabilities(ctx, contest_round_id);
    if (!contest_round_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_contest_round_and_user_submissions(
        ctx.session,
        contest_round_info_for_caps->session_user_contest_user_mode,
        user_id,
        contest_round_info_for_caps->contest_round_full_results,
        utc_mysql_datetime()
    );
    if (!caps.query_with_type_contest_problem_final) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        FIRST_QUERY_LIMIT,
        Condition(
            concat_tostr(
                "s.contest_round_id=? AND s.user_id=? AND s.",
                caps.query_with_type_contest_problem_final_show_full_instead_of_initial_results
                    ? "contest_problem_final"
                    : "contest_problem_initial_final",
                "=1"
            ),
            contest_round_id,
            user_id
        )
    );
}

Response list_contest_round_and_user_submissions_with_type_contest_problem_final_below_id(
    Context& ctx,
    decltype(ContestRound::id) contest_round_id,
    decltype(User::id) user_id,
    decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto contest_round_info_for_caps =
        get_contest_round_info_for_capabilities(ctx, contest_round_id);
    if (!contest_round_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_contest_round_and_user_submissions(
        ctx.session,
        contest_round_info_for_caps->session_user_contest_user_mode,
        user_id,
        contest_round_info_for_caps->contest_round_full_results,
        utc_mysql_datetime()
    );
    if (!caps.query_with_type_contest_problem_final) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition(
            concat_tostr(
                "s.contest_round_id=? AND s.user_id=? AND s.",
                caps.query_with_type_contest_problem_final_show_full_instead_of_initial_results
                    ? "contest_problem_final"
                    : "contest_problem_initial_final",
                "=1 AND s.id<?"
            ),
            contest_round_id,
            user_id,
            submission_id
        )
    );
}

Response list_contest_round_and_user_submissions_with_type_ignored(
    Context& ctx, decltype(ContestRound::id) contest_round_id, decltype(User::id) user_id
) {
    STACK_UNWINDING_MARK;

    auto contest_round_info_for_caps =
        get_contest_round_info_for_capabilities(ctx, contest_round_id);
    if (!contest_round_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_contest_round_and_user_submissions(
        ctx.session,
        contest_round_info_for_caps->session_user_contest_user_mode,
        user_id,
        contest_round_info_for_caps->contest_round_full_results,
        utc_mysql_datetime()
    );
    if (!caps.query_with_type_ignored) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        FIRST_QUERY_LIMIT,
        Condition(
            "s.contest_round_id=? AND s.user_id=? AND s.type=?",
            contest_round_id,
            user_id,
            Submission::Type::IGNORED
        )
    );
}

Response list_contest_round_and_user_submissions_with_type_ignored_below_id(
    Context& ctx,
    decltype(ContestRound::id) contest_round_id,
    decltype(User::id) user_id,
    decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto contest_round_info_for_caps =
        get_contest_round_info_for_capabilities(ctx, contest_round_id);
    if (!contest_round_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_contest_round_and_user_submissions(
        ctx.session,
        contest_round_info_for_caps->session_user_contest_user_mode,
        user_id,
        contest_round_info_for_caps->contest_round_full_results,
        utc_mysql_datetime()
    );
    if (!caps.query_with_type_ignored) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition(
            "s.contest_round_id=? AND s.user_id=? AND s.type=? AND s.id<?",
            contest_round_id,
            user_id,
            Submission::Type::IGNORED,
            submission_id
        )
    );
}

Response
list_contest_problem_submissions(Context& ctx, decltype(ContestProblem::id) contest_problem_id) {
    STACK_UNWINDING_MARK;

    auto contest_problem_info_for_caps =
        get_contest_problem_info_for_capabilities(ctx, contest_problem_id);
    if (!contest_problem_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_contest_problem_submissions(
        ctx.session, contest_problem_info_for_caps->session_user_contest_user_mode
    );
    if (!caps.query_all) {
        return ctx.response_403();
    }
    return do_list(ctx, FIRST_QUERY_LIMIT, Condition("s.contest_problem_id=?", contest_problem_id));
}

Response list_contest_problem_submissions_below_id(
    Context& ctx,
    decltype(ContestProblem::id) contest_problem_id,
    decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto contest_problem_info_for_caps =
        get_contest_problem_info_for_capabilities(ctx, contest_problem_id);
    if (!contest_problem_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_contest_problem_submissions(
        ctx.session, contest_problem_info_for_caps->session_user_contest_user_mode
    );
    if (!caps.query_all) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition("s.contest_problem_id=? AND s.id<?", contest_problem_id, submission_id)
    );
}

Response list_contest_problem_submissions_with_type_contest_problem_final(
    Context& ctx, decltype(ContestProblem::id) contest_problem_id
) {
    STACK_UNWINDING_MARK;

    auto contest_problem_info_for_caps =
        get_contest_problem_info_for_capabilities(ctx, contest_problem_id);
    if (!contest_problem_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_contest_problem_submissions(
        ctx.session, contest_problem_info_for_caps->session_user_contest_user_mode
    );
    if (!caps.query_with_type_contest_problem_final) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        FIRST_QUERY_LIMIT,
        Condition("s.contest_problem_id=? AND s.contest_problem_final=1", contest_problem_id)
    );
}

Response list_contest_problem_submissions_with_type_contest_problem_final_below_id(
    Context& ctx,
    decltype(ContestProblem::id) contest_problem_id,
    decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto contest_problem_info_for_caps =
        get_contest_problem_info_for_capabilities(ctx, contest_problem_id);
    if (!contest_problem_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_contest_problem_submissions(
        ctx.session, contest_problem_info_for_caps->session_user_contest_user_mode
    );
    if (!caps.query_with_type_contest_problem_final) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition(
            "s.contest_problem_id=? AND s.contest_problem_final=1 AND s.id<?",
            contest_problem_id,
            submission_id
        )
    );
}

Response list_contest_problem_submissions_with_type_ignored(
    Context& ctx, decltype(ContestProblem::id) contest_problem_id
) {
    STACK_UNWINDING_MARK;

    auto contest_problem_info_for_caps =
        get_contest_problem_info_for_capabilities(ctx, contest_problem_id);
    if (!contest_problem_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_contest_problem_submissions(
        ctx.session, contest_problem_info_for_caps->session_user_contest_user_mode
    );
    if (!caps.query_with_type_ignored) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        FIRST_QUERY_LIMIT,
        Condition(
            "s.contest_problem_id=? AND s.type=?", contest_problem_id, Submission::Type::IGNORED
        )
    );
}

Response list_contest_problem_submissions_with_type_ignored_below_id(
    Context& ctx,
    decltype(ContestProblem::id) contest_problem_id,
    decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto contest_problem_info_for_caps =
        get_contest_problem_info_for_capabilities(ctx, contest_problem_id);
    if (!contest_problem_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_contest_problem_submissions(
        ctx.session, contest_problem_info_for_caps->session_user_contest_user_mode
    );
    if (!caps.query_with_type_ignored) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition(
            "s.contest_problem_id=? AND s.type=? AND s.id<?",
            contest_problem_id,
            Submission::Type::IGNORED,
            submission_id
        )
    );
}

Response list_contest_problem_and_user_submissions(
    Context& ctx, decltype(ContestProblem::id) contest_problem_id, decltype(User::id) user_id
) {
    STACK_UNWINDING_MARK;

    auto contest_problem_info_for_caps =
        get_contest_problem_info_for_capabilities(ctx, contest_problem_id);
    if (!contest_problem_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_contest_problem_and_user_submissions(
        ctx.session,
        contest_problem_info_for_caps->session_user_contest_user_mode,
        user_id,
        contest_problem_info_for_caps->contest_round_full_results,
        utc_mysql_datetime()
    );
    if (!caps.query_all) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        FIRST_QUERY_LIMIT,
        Condition("s.contest_problem_id=? AND s.user_id=?", contest_problem_id, user_id)
    );
}

Response list_contest_problem_and_user_submissions_below_id(
    Context& ctx,
    decltype(ContestProblem::id) contest_problem_id,
    decltype(User::id) user_id,
    decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto contest_problem_info_for_caps =
        get_contest_problem_info_for_capabilities(ctx, contest_problem_id);
    if (!contest_problem_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_contest_problem_and_user_submissions(
        ctx.session,
        contest_problem_info_for_caps->session_user_contest_user_mode,
        user_id,
        contest_problem_info_for_caps->contest_round_full_results,
        utc_mysql_datetime()
    );
    if (!caps.query_all) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition(
            "s.contest_problem_id=? AND s.user_id=? AND s.id<?",
            contest_problem_id,
            user_id,
            submission_id
        )
    );
}

Response list_contest_problem_and_user_submissions_with_type_contest_problem_final(
    Context& ctx, decltype(ContestProblem::id) contest_problem_id, decltype(User::id) user_id
) {
    STACK_UNWINDING_MARK;

    auto contest_problem_info_for_caps =
        get_contest_problem_info_for_capabilities(ctx, contest_problem_id);
    if (!contest_problem_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_contest_problem_and_user_submissions(
        ctx.session,
        contest_problem_info_for_caps->session_user_contest_user_mode,
        user_id,
        contest_problem_info_for_caps->contest_round_full_results,
        utc_mysql_datetime()
    );
    if (!caps.query_with_type_contest_problem_final) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        FIRST_QUERY_LIMIT,
        Condition(
            concat_tostr(
                "s.contest_problem_id=? AND s.user_id=? AND s.",
                caps.query_with_type_contest_problem_final_show_full_instead_of_initial_results
                    ? "contest_problem_final"
                    : "contest_problem_initial_final",
                "=1"
            ),
            contest_problem_id,
            user_id
        )
    );
}

Response list_contest_problem_and_user_submissions_with_type_ignored(
    Context& ctx, decltype(ContestProblem::id) contest_problem_id, decltype(User::id) user_id
) {
    STACK_UNWINDING_MARK;

    auto contest_problem_info_for_caps =
        get_contest_problem_info_for_capabilities(ctx, contest_problem_id);
    if (!contest_problem_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_contest_problem_and_user_submissions(
        ctx.session,
        contest_problem_info_for_caps->session_user_contest_user_mode,
        user_id,
        contest_problem_info_for_caps->contest_round_full_results,
        utc_mysql_datetime()
    );
    if (!caps.query_with_type_ignored) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        FIRST_QUERY_LIMIT,
        Condition(
            "s.contest_problem_id=? AND s.user_id=? AND s.type=?",
            contest_problem_id,
            user_id,
            Submission::Type::IGNORED
        )
    );
}

Response list_contest_problem_and_user_submissions_with_type_ignored_below_id(
    Context& ctx,
    decltype(ContestProblem::id) contest_problem_id,
    decltype(User::id) user_id,
    decltype(Submission::id) submission_id
) {
    STACK_UNWINDING_MARK;

    auto contest_problem_info_for_caps =
        get_contest_problem_info_for_capabilities(ctx, contest_problem_id);
    if (!contest_problem_info_for_caps) {
        return ctx.response_404();
    }
    auto caps = capabilities::list_contest_problem_and_user_submissions(
        ctx.session,
        contest_problem_info_for_caps->session_user_contest_user_mode,
        user_id,
        contest_problem_info_for_caps->contest_round_full_results,
        utc_mysql_datetime()
    );
    if (!caps.query_with_type_ignored) {
        return ctx.response_403();
    }
    return do_list(
        ctx,
        NEXT_QUERY_LIMIT,
        Condition(
            "s.contest_problem_id=? AND s.user_id=? AND s.type=? AND s.id<?",
            contest_problem_id,
            user_id,
            Submission::Type::IGNORED,
            submission_id
        )
    );
}

Response view_submission(Context& ctx, decltype(Submission::id) submission_id) {
    STACK_UNWINDING_MARK;

    SubmissionInfo s;
    s.id = submission_id;
    decltype(Problem::visibility) problem_visibility;
    decltype(Problem::owner_id) problem_owner_id;
    std::optional<decltype(ContestProblem::method_of_choosing_final_submission)>
        contest_problem_method_of_choosing_final_submission;
    std::optional<decltype(ContestProblem::score_revealing)> contest_problem_score_revealing;
    std::optional<decltype(ContestRound::full_results)> contest_round_full_results;
    std::optional<decltype(ContestUser::mode)> session_user_contest_user_mode;
    {
        auto stmt =
            ctx.mysql.execute(Select("s.user_id, s.type, p.visibility, p.owner_id, "
                                     "cp.method_of_choosing_final_submission, "
                                     "cp.score_revealing, cr.full_results, cu.mode")
                                  .from("submissions s")
                                  .left_join("problems p") // left_join is faster than inner_join
                                  .on("p.id=s.problem_id")
                                  .left_join("contest_problems cp")
                                  .on("cp.id=s.contest_problem_id")
                                  .left_join("contest_rounds cr")
                                  .on("cr.id=s.contest_round_id")
                                  .left_join("contest_users cu")
                                  .on("cu.contest_id=s.contest_id AND cu.user_id=?",
                                      ctx.session ? optional{ctx.session->user_id} : std::nullopt)
                                  .where("s.id=?", submission_id));
        stmt.res_bind(
            s.user_id,
            s.type,
            problem_visibility,
            problem_owner_id,
            contest_problem_method_of_choosing_final_submission,
            contest_problem_score_revealing,
            contest_round_full_results,
            session_user_contest_user_mode
        );
        if (!stmt.next()) {
            return ctx.response_404();
        }
    }
    auto caps = capabilities::submission(
        ctx.session,
        s.user_id,
        s.type,
        problem_visibility,
        problem_owner_id,
        session_user_contest_user_mode,
        contest_round_full_results,
        contest_problem_method_of_choosing_final_submission,
        contest_problem_score_revealing,
        utc_mysql_datetime()
    );
    if (!caps.view) {
        return ctx.response_403();
    }

    auto stmt = ctx.mysql.execute(
        Select("s.created_at, u.username, u.first_name, u.last_name, s.problem_id, p.name, "
               "s.contest_problem_id, cp.name, s.contest_round_id, cr.name, s.contest_id, c.name, "
               "s.language, s.problem_final, s.contest_problem_final, "
               "s.contest_problem_initial_final, s.initial_status, s.full_status, s.score, "
               "s.initial_report, s.final_report")
            .from("submissions s")
            .left_join("users u")
            .on("u.id=s.user_id")
            .left_join("problems p") // left_join is faster than inner_join
            .on("p.id=s.problem_id")
            .left_join("contest_problems cp")
            .on("cp.id=s.contest_problem_id")
            .left_join("contest_rounds cr")
            .on("cr.id=s.contest_round_id")
            .left_join("contests c")
            .on("c.id=s.contest_id")
            .where("s.id=?", submission_id)
    );
    decltype(Submission::initial_report) initial_judgement_protocol;
    decltype(Submission::final_report) full_judgement_protocol;
    stmt.res_bind(
        s.created_at,
        s.user_username,
        s.user_first_name,
        s.user_last_name,
        s.problem_id,
        s.problem_name,
        s.contest_problem_id,
        s.contest_problem_name,
        s.contest_round_id,
        s.contest_round_name,
        s.contest_id,
        s.contest_name,
        s.language,
        s.is_problem_final,
        s.is_contest_problem_final,
        s.is_contest_problem_initial_final,
        s.initial_status,
        s.full_status,
        s.score,
        initial_judgement_protocol,
        full_judgement_protocol
    );
    throw_assert(stmt.next());

    json_str::Object obj;
    s.append_to(obj, caps);
    if (caps.view_initial_judgement_protocol) {
        obj.prop("initial_judgement_protocol", initial_judgement_protocol);
    }
    if (caps.view_full_judgement_protocol) {
        obj.prop("full_judgement_protocol", full_judgement_protocol);
    }
    return ctx.response_json(std::move(obj).into_str());
}

} // namespace web_server::submissions::api

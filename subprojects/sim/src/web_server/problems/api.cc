#include "../capabilities/problems.hh"
#include "../http/form_validation.hh"
#include "../http/response.hh"
#include "../web_worker/context.hh"
#include "api.hh"

#include <chrono>
#include <cstdint>
#include <optional>
#include <sim/jobs/job.hh>
#include <sim/problem_tags/problem_tag.hh>
#include <sim/problems/problem.hh>
#include <sim/sql/sql.hh>
#include <sim/submissions/submission.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/config_file.hh>
#include <simlib/enum_to_underlying_type.hh>
#include <simlib/file_path.hh>
#include <simlib/json_str/json_str.hh>
#include <simlib/string_traits.hh>
#include <simlib/string_transform.hh>
#include <simlib/string_view.hh>
#include <simlib/time.hh>
#include <simlib/time_format_conversions.hh>
#include <utility>

using sim::jobs::Job;
using sim::problem_tags::ProblemTag;
using sim::problems::Problem;
using sim::sql::Condition;
using sim::sql::InsertInto;
using sim::sql::Select;
using sim::submissions::Submission;
using sim::users::User;
using std::optional;
using web_server::capabilities::ProblemsListCapabilities;
using web_server::http::Response;
using web_server::web_worker::Context;

namespace capabilities = web_server::capabilities;

namespace {

struct ProblemInfo {
    decltype(Problem::id) id{};
    decltype(Problem::type) type{};
    decltype(Problem::name) name;
    decltype(Problem::label) label;
    std::optional<decltype(Problem::owner_id)::value_type> owner_id;
    std::optional<std::string> owner_username;
    std::optional<std::string> owner_first_name;
    std::optional<std::string> owner_last_name;
    decltype(Problem::created_at) created_at;
    decltype(Problem::updated_at) updated_at;
    std::optional<decltype(Submission::full_status)> final_submission_full_status;

    explicit ProblemInfo() = default;
    ProblemInfo(const ProblemInfo&) = delete;
    ProblemInfo(ProblemInfo&&) = delete;
    ProblemInfo& operator=(const ProblemInfo&) = delete;
    ProblemInfo& operator=(ProblemInfo&&) = delete;
    ~ProblemInfo() = default;

    void append_to(
        const decltype(Context::session)& session,
        json_str::ObjectBuilder& obj,
        const std::vector<decltype(ProblemTag::name)>& public_tags,
        const std::vector<decltype(ProblemTag::name)>& hidden_tags
    ) {
        const auto caps = capabilities::problem(session, type, owner_id);
        throw_assert(caps.view);
        obj.prop("id", id);
        obj.prop("type", type);
        obj.prop("name", name);
        obj.prop("label", label);
        if (caps.view_owner) {
            if (owner_id) {
                obj.prop_obj("owner", [&](auto& obj) {
                    obj.prop("id", owner_id);
                    obj.prop("username", owner_username);
                    obj.prop("first_name", owner_first_name);
                    obj.prop("last_name", owner_last_name);
                });
            } else {
                obj.prop("owner", nullptr);
            }
        }
        if (caps.view_creation_time) {
            obj.prop("created_at", created_at);
        }
        if (caps.view_update_time) {
            obj.prop("updated_at", updated_at);
        }
        if (caps.view_final_submission_full_status) {
            obj.prop("final_submission_full_status", final_submission_full_status);
        }
        obj.prop_obj("tags", [&](auto& obj) {
            if (caps.view_public_tags) {
                obj.prop_arr("public", [&](auto& arr) {
                    for (auto& tag_name : public_tags) {
                        arr.val(tag_name);
                    }
                });
            }
            if (caps.view_hidden_tags) {
                obj.prop_arr("hidden", [&](auto& arr) {
                    for (auto& tag_name : hidden_tags) {
                        arr.val(tag_name);
                    }
                });
            }
        });
        obj.prop_obj("capabilities", [&](auto& obj) {
            obj.prop("view", caps.view);
            obj.prop("view_statement", caps.view_statement);
            obj.prop("view_public_tags", caps.view_public_tags);
            obj.prop("view_hidden_tags", caps.view_hidden_tags);
            obj.prop("view_solutions", caps.view_solutions);
            obj.prop("view_simfile", caps.view_simfile);
            obj.prop("view_owner", caps.view_owner);
            obj.prop("view_creation_time", caps.view_creation_time);
            obj.prop("view_update_time", caps.view_update_time);
            obj.prop("view_final_submission_full_status", caps.view_final_submission_full_status);
            obj.prop("download", caps.download);
            obj.prop("create_submission", caps.create_submission);
            obj.prop("edit", caps.edit);
            obj.prop("reupload", caps.reupload);
            obj.prop("rejudge_all_submissions", caps.rejudge_all_submissions);
            obj.prop("reset_time_limits", caps.reset_time_limits);
            obj.prop("delete", caps.delete_);
            obj.prop("merge_into_another_problem", caps.merge_into_another_problem);
            obj.prop(
                "merge_other_problem_into_this_problem", caps.merge_other_problem_into_this_problem
            );
        });
    }
};

template <class... Params>
Response do_list(Context& ctx, uint32_t limit, Condition<Params...>&& where_cond) {
    STACK_UNWINDING_MARK;

    ProblemInfo p;
    auto stmt = ctx.mysql.execute(
        Select("p.id, p.type, p.name, p.label, p.owner_id, u.username, u.first_name, u.last_name, "
               "p.created_at, p.updated_at, s.full_status")
            .from("problems p")
            .left_join("users u")
            .on("u.id=p.owner_id")
            .left_join("submissions s")
            .on(Condition("s.problem_id=p.id") &&
                Condition(
                    "s.owner=?", ctx.session ? optional{ctx.session->user_id} : std::nullopt
                ) &&
                Condition("s.problem_final IS TRUE"))
            .where(Condition{where_cond})
            .order_by("p.id DESC")
            .limit("?", limit)
    );
    stmt.res_bind(
        p.id,
        p.type,
        p.name,
        p.label,
        p.owner_id,
        p.owner_username,
        p.owner_first_name,
        p.owner_last_name,
        p.created_at,
        p.updated_at,
        p.final_submission_full_status
    );

    std::vector<decltype(ProblemTag::name)> public_tags;
    std::vector<decltype(ProblemTag::name)> hidden_tags;
    auto tags_stmt = ctx.mysql.execute(Select("pt.problem_id, pt.name, pt.is_hidden")
                                           .from("problem_tags pt")
                                           .inner_join(
                                               Select("p.id AS id")
                                                   .from("problems p")
                                                   .where(std::move(where_cond))
                                                   .order_by("id DESC")
                                                   .limit("?", limit),
                                               "pids"
                                           )
                                           .on("pids.id=pt.problem_id")
                                           .order_by("pt.problem_id DESC, pt.is_hidden, pt.name"));
    decltype(ProblemTag::problem_id) tag_problem_id{};
    decltype(ProblemTag::name) tag_name;
    decltype(ProblemTag::is_hidden) tag_is_hidden;
    tags_stmt.res_bind(tag_problem_id, tag_name, tag_is_hidden);
    auto fill_tags_for_problem = [&, not_done = tags_stmt.next()](decltype(Problem::id) problem_id
                                 ) mutable {
        public_tags.clear();
        hidden_tags.clear();
        while (not_done) {
            if (tag_problem_id != problem_id) {
                break;
            }
            (tag_is_hidden ? hidden_tags : public_tags).emplace_back(tag_name);
            not_done = tags_stmt.next();
        }
    };

    json_str::Object obj;
    size_t rows_num = 0;
    obj.prop_arr("list", [&](auto& arr) {
        while (stmt.next()) {
            ++rows_num;
            fill_tags_for_problem(p.id);
            arr.val_obj([&](auto& obj) { p.append_to(ctx.session, obj, public_tags, hidden_tags); }
            );
        }
    });
    obj.prop("may_be_more", rows_num == limit);
    return ctx.response_json(std::move(obj).into_str());
}

constexpr bool
is_query_allowed(ProblemsListCapabilities caps, optional<decltype(Problem::type)> problem_type) {
    if (not problem_type) {
        return caps.query_all;
    }
    // NOLINTNEXTLINE(bugprone-switch-missing-default-case)
    switch (*problem_type) {
    case Problem::Type::PUBLIC: return caps.query_with_type_public;
    case Problem::Type::CONTEST_ONLY: return caps.query_with_type_contest_only;
    case Problem::Type::PRIVATE: return caps.query_with_type_private;
    }
    THROW("unexpected problem type");
}

Condition<>
caps_to_condition(ProblemsListCapabilities caps, optional<decltype(Problem::type)> problem_type) {
    optional<Condition<>> res;
    if (caps.view_all_with_type_public and (!problem_type or problem_type == Problem::Type::PUBLIC))
    {
        res = std::move(res) ||
            optional{
                Condition(concat_tostr("p.type=", enum_to_underlying_type(Problem::Type::PUBLIC)))
            };
    }
    if (caps.view_all_with_type_contest_only and
        (!problem_type or problem_type == Problem::Type::CONTEST_ONLY))
    {
        res = std::move(res) ||
            optional{Condition(
                concat_tostr("p.type=", enum_to_underlying_type(Problem::Type::CONTEST_ONLY))
            )};
    }
    if (caps.view_all_with_type_private and
        (!problem_type or problem_type == Problem::Type::PRIVATE))
    {
        res = std::move(res) ||
            optional{
                Condition(concat_tostr("p.type=", enum_to_underlying_type(Problem::Type::PRIVATE)))
            };
    }
    return std::move(res).value_or(Condition("FALSE"));
}

template <class... Params>
Response do_list_problems(
    Context& ctx,
    uint32_t limit,
    optional<decltype(Problem::type)> problem_type,
    Condition<Params...>&& where_cond
) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_problems(ctx.session);
    if (not is_query_allowed(caps, problem_type)) {
        return ctx.response_403();
    }

    if (ctx.session) {
        auto user_caps = capabilities::list_user_problems(ctx.session, ctx.session->user_id);
        return do_list(
            ctx,
            limit,
            std::move(where_cond) &&
                (caps_to_condition(caps, problem_type) ||
                 (Condition("p.owner_id=?", ctx.session->user_id) &&
                  caps_to_condition(user_caps, problem_type)))
        );
    }
    return do_list(ctx, limit, std::move(where_cond) && caps_to_condition(caps, problem_type));
}

template <class... Params>
Response do_list_user_problems(
    Context& ctx,
    uint32_t limit,
    decltype(User::id) user_id,
    optional<decltype(Problem::type)> problem_type,
    Condition<Params...>&& where_cond
) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_user_problems(ctx.session, user_id);
    if (not is_query_allowed(caps, problem_type)) {
        return ctx.response_403();
    }

    return do_list(
        ctx,
        limit,
        std::move(where_cond) && Condition("p.owner_id=?", user_id) &&
            caps_to_condition(caps, problem_type)
    );
}

} // namespace

namespace web_server::problems::api {

constexpr inline uint32_t FIRST_QUERY_LIMIT = 64;
constexpr inline uint32_t NEXT_QUERY_LIMIT = 200;

Response list_problems(Context& ctx) {
    STACK_UNWINDING_MARK;
    return do_list_problems(ctx, FIRST_QUERY_LIMIT, std::nullopt, Condition("TRUE"));
}

Response list_problems_below_id(Context& ctx, decltype(Problem::id) problem_id) {
    STACK_UNWINDING_MARK;
    return do_list_problems(ctx, NEXT_QUERY_LIMIT, std::nullopt, Condition("p.id<?", problem_id));
}

Response list_problems_with_type(Context& ctx, decltype(Problem::type) problem_type) {
    STACK_UNWINDING_MARK;
    return do_list_problems(ctx, FIRST_QUERY_LIMIT, problem_type, Condition("TRUE"));
}

Response list_problems_with_type_below_id(
    Context& ctx, decltype(Problem::type) problem_type, decltype(Problem::id) problem_id
) {
    STACK_UNWINDING_MARK;
    return do_list_problems(ctx, NEXT_QUERY_LIMIT, problem_type, Condition("p.id<?", problem_id));
}

Response list_user_problems(Context& ctx, decltype(User::id) user_id) {
    STACK_UNWINDING_MARK;
    return do_list_user_problems(ctx, FIRST_QUERY_LIMIT, user_id, std::nullopt, Condition("TRUE"));
}

Response list_user_problems_below_id(
    Context& ctx, decltype(User::id) user_id, decltype(Problem::id) problem_id
) {
    STACK_UNWINDING_MARK;
    return do_list_user_problems(
        ctx, NEXT_QUERY_LIMIT, user_id, std::nullopt, Condition("p.id<?", problem_id)
    );
}

Response list_user_problems_with_type(
    Context& ctx, decltype(User::id) user_id, decltype(Problem::type) problem_type
) {
    STACK_UNWINDING_MARK;
    return do_list_user_problems(ctx, FIRST_QUERY_LIMIT, user_id, problem_type, Condition("TRUE"));
}

Response list_user_problems_with_type_below_id(
    Context& ctx,
    decltype(User::id) user_id,
    decltype(Problem::type) problem_type,
    decltype(Problem::id) problem_id
) {
    STACK_UNWINDING_MARK;
    return do_list_user_problems(
        ctx, NEXT_QUERY_LIMIT, user_id, problem_type, Condition("p.id<?", problem_id)
    );
}

Response view_problem(Context& ctx, decltype(Problem::id) problem_id) {
    STACK_UNWINDING_MARK;

    ProblemInfo p;
    p.id = problem_id;
    auto stmt =
        ctx.mysql.execute(Select("type, owner_id").from("problems").where("id=?", problem_id));
    stmt.res_bind(p.type, p.owner_id);
    if (not stmt.next()) {
        return ctx.response_404();
    }
    auto caps = capabilities::problem(ctx.session, p.type, p.owner_id);
    if (not caps.view) {
        return ctx.response_403();
    }
    stmt = ctx.mysql.execute(
        Select("p.name, p.label, u.username, u.first_name, u.last_name, p.created_at, "
               "p.updated_at, s.full_status, p.simfile")
            .from("problems p")
            .left_join("users u")
            .on("u.id=p.owner_id")
            .left_join("submissions s")
            .on(Condition("s.problem_id=p.id") &&
                Condition(
                    "s.owner=?", ctx.session ? optional{ctx.session->user_id} : std::nullopt
                ) &&
                Condition("s.problem_final IS TRUE"))
            .where("p.id=?", problem_id)
    );
    decltype(Problem::simfile) simfile;
    stmt.res_bind(
        p.name,
        p.label,
        p.owner_username,
        p.owner_first_name,
        p.owner_last_name,
        p.created_at,
        p.updated_at,
        p.final_submission_full_status,
        simfile
    );
    throw_assert(stmt.next());

    std::vector<decltype(ProblemTag::name)> public_tags;
    std::vector<decltype(ProblemTag::name)> hidden_tags;
    {
        auto tags_stmt = ctx.mysql.execute(Select("name, is_hidden")
                                               .from("problem_tags")
                                               .where("problem_id=?", p.id)
                                               .order_by("is_hidden, name"));
        decltype(ProblemTag::name) tag_name;
        decltype(ProblemTag::is_hidden) tag_is_hidden;
        tags_stmt.res_bind(tag_name, tag_is_hidden);
        while (tags_stmt.next()) {
            (tag_is_hidden ? hidden_tags : public_tags).emplace_back(tag_name);
        }
    }

    json_str::Object obj;
    p.append_to(ctx.session, obj, public_tags, hidden_tags);
    if (caps.view_simfile) {
        obj.prop("simfile", simfile);
        ConfigFile cf;
        cf.add_vars("memory_limit");
        cf.load_config_from_string(simfile);
        obj.prop("default_memory_limit", cf.get_var("memory_limit").as<uint64_t>().value());
        // TODO: default_memory_limit should be a separate column in the problems table
    }
    return ctx.response_json(std::move(obj).into_str());
}

} // namespace web_server::problems::api

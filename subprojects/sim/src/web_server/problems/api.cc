#include "../capabilities/problems.hh"
#include "../http/form_validation.hh"
#include "../http/response.hh"
#include "../web_worker/context.hh"
#include "api.hh"

#include <chrono>
#include <cstdint>
#include <optional>
#include <sim/internal_files/internal_file.hh>
#include <sim/jobs/job.hh>
#include <sim/judging_config.hh>
#include <sim/problem_tags/problem_tag.hh>
#include <sim/problems/problem.hh>
#include <sim/sql/sql.hh>
#include <sim/submissions/submission.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/config_file.hh>
#include <simlib/enum_to_underlying_type.hh>
#include <simlib/file_manip.hh>
#include <simlib/file_path.hh>
#include <simlib/file_remover.hh>
#include <simlib/json_str/json_str.hh>
#include <simlib/macros/stack_unwinding.hh>
#include <simlib/result.hh>
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
using web_server::http::ApiParam;
using web_server::http::Response;
using web_server::web_worker::Context;

namespace capabilities = web_server::capabilities;

namespace {

struct ProblemInfo {
    decltype(Problem::id) id{};
    decltype(Problem::visibility) visibility{};
    decltype(Problem::name) name;
    decltype(Problem::label) label;
    optional<decltype(Problem::owner_id)::value_type> owner_id;
    optional<std::string> owner_username;
    optional<std::string> owner_first_name;
    optional<std::string> owner_last_name;
    decltype(Problem::created_at) created_at;
    decltype(Problem::updated_at) updated_at;
    optional<decltype(Submission::full_status)> final_submission_full_status;

    explicit ProblemInfo() = default;
    ProblemInfo(const ProblemInfo&) = delete;
    ProblemInfo(ProblemInfo&&) = delete;
    ProblemInfo& operator=(const ProblemInfo&) = delete;
    ProblemInfo& operator=(ProblemInfo&&) = delete;
    ~ProblemInfo() = default;

    void append_to(
        const capabilities::ProblemCapabilities& caps,
        json_str::ObjectBuilder& obj,
        const std::vector<decltype(ProblemTag::name)>& public_tags,
        const std::vector<decltype(ProblemTag::name)>& hidden_tags
    ) {
        throw_assert(caps.view);
        obj.prop("id", id);
        obj.prop("visibility", visibility);
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
        Select("p.id, p.visibility, p.name, p.label, p.owner_id, u.username, u.first_name, "
               "u.last_name, p.created_at, p.updated_at, s.full_status")
            .from("problems p")
            .left_join("users u")
            .on("u.id=p.owner_id")
            .left_join("submissions s")
            .on(Condition("s.problem_id=p.id") &&
                Condition(
                    "s.user_id=?", ctx.session ? optional{ctx.session->user_id} : std::nullopt
                ) &&
                Condition("s.problem_final IS TRUE"))
            .where(Condition{where_cond})
            .order_by("p.id DESC")
            .limit("?", limit)
    );
    stmt.res_bind(
        p.id,
        p.visibility,
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
            arr.val_obj([&](auto& obj) {
                p.append_to(
                    capabilities::problem(ctx.session, p.visibility, p.owner_id),
                    obj,
                    public_tags,
                    hidden_tags
                );
            });
        }
    });
    obj.prop("may_be_more", rows_num == limit);
    return ctx.response_json(std::move(obj).into_str());
}

constexpr bool is_query_allowed(
    ProblemsListCapabilities caps, optional<decltype(Problem::visibility)> problem_visibility
) {
    if (not problem_visibility) {
        return caps.query_all;
    }
    // NOLINTNEXTLINE(bugprone-switch-missing-default-case)
    switch (*problem_visibility) {
    case Problem::Visibility::PUBLIC: return caps.query_with_visibility_public;
    case Problem::Visibility::CONTEST_ONLY: return caps.query_with_visibility_contest_only;
    case Problem::Visibility::PRIVATE: return caps.query_with_visibility_private;
    }
    THROW("unexpected problem visibility");
}

Condition<> caps_to_condition(
    ProblemsListCapabilities caps, optional<decltype(Problem::visibility)> problem_visibility
) {
    optional<Condition<>> res;
    if (caps.view_all_with_visibility_public and
        (!problem_visibility or problem_visibility == Problem::Visibility::PUBLIC))
    {
        res = std::move(res) ||
            optional{Condition(
                concat_tostr("p.visibility=", enum_to_underlying_type(Problem::Visibility::PUBLIC))
            )};
    }
    if (caps.view_all_with_visibility_contest_only and
        (!problem_visibility or problem_visibility == Problem::Visibility::CONTEST_ONLY))
    {
        res = std::move(res) ||
            optional{Condition(concat_tostr(
                "p.visibility=", enum_to_underlying_type(Problem::Visibility::CONTEST_ONLY)
            ))};
    }
    if (caps.view_all_with_visibility_private and
        (!problem_visibility or problem_visibility == Problem::Visibility::PRIVATE))
    {
        res = std::move(res) ||
            optional{Condition(
                concat_tostr("p.visibility=", enum_to_underlying_type(Problem::Visibility::PRIVATE))
            )};
    }
    return std::move(res).value_or(Condition("FALSE"));
}

template <class... Params>
Response do_list_problems(
    Context& ctx,
    uint32_t limit,
    optional<decltype(Problem::visibility)> problem_visibility,
    Condition<Params...>&& where_cond
) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_problems(ctx.session);
    if (not is_query_allowed(caps, problem_visibility)) {
        return ctx.response_403();
    }

    if (ctx.session) {
        auto user_caps = capabilities::list_user_problems(ctx.session, ctx.session->user_id);
        return do_list(
            ctx,
            limit,
            std::move(where_cond) &&
                (caps_to_condition(caps, problem_visibility) ||
                 (Condition("p.owner_id=?", ctx.session->user_id) &&
                  caps_to_condition(user_caps, problem_visibility)))
        );
    }
    return do_list(
        ctx, limit, std::move(where_cond) && caps_to_condition(caps, problem_visibility)
    );
}

template <class... Params>
Response do_list_user_problems(
    Context& ctx,
    uint32_t limit,
    decltype(User::id) user_id,
    optional<decltype(Problem::visibility)> problem_visibility,
    Condition<Params...>&& where_cond
) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::list_user_problems(ctx.session, user_id);
    if (not is_query_allowed(caps, problem_visibility)) {
        return ctx.response_403();
    }

    return do_list(
        ctx,
        limit,
        std::move(where_cond) && Condition("p.owner_id=?", user_id) &&
            caps_to_condition(caps, problem_visibility)
    );
}

optional<capabilities::ProblemCapabilities>
get_problem_caps(Context& ctx, decltype(Problem::id) problem_id) {
    auto stmt =
        ctx.mysql.execute(Select("visibility, owner_id").from("problems").where("id=?", problem_id)
        );
    decltype(Problem::visibility) visibility;
    decltype(Problem::owner_id) owner_id;
    stmt.res_bind(visibility, owner_id);
    if (stmt.next()) {
        return capabilities::problem(ctx.session, visibility, owner_id);
    }
    return std::nullopt;
}

namespace params {

constexpr ApiParam visibility{&Problem::visibility, "visibility", "Visibility"};
constexpr ApiParam<bool> ignore_simfile{"ignore_simfile", "Ignore simfile"};
constexpr ApiParam<web_server::http::SubmittedFile> package{"package", "Zipped package"};
constexpr ApiParam name{&Problem::name, "name", "Name"};
constexpr ApiParam label{&Problem::label, "label", "Label"};
constexpr ApiParam<CStringView> memory_limit_in_mib_str{"memory_limit_in_mib", "Memory limit"};

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#endif
ENUM_WITH_STRING_CONVERSIONS(TimeLimitsKind, uint8_t,
    (KEEP_IF_POSSIBLE, 1, "keep_if_possible")
    (RESET, 2, "reset")
    (FIXED, 3, "fixed")
);
#ifdef __clang__
#pragma clang diagnostic pop
#endif

constexpr ApiParam<TimeLimitsKind> time_limits{"time_limits", "Time limits"};
constexpr ApiParam<CStringView> fixed_time_limit_in_nsec_str{
    "fixed_time_limit_in_nsec", "Fixed time limit"
};
constexpr ApiParam<bool> reset_scoring{"reset_scoring", "Reset scoring"};
constexpr ApiParam<bool> look_for_new_tests{"look_for_new_tests", "Look for new tests"};

} // namespace params

struct AddProblemAndReuploadProblemCommonParams {
    bool ignore_simfile;
    web_server::http::SubmittedFile package;
    decltype(Problem::name) name;
    decltype(Problem::label) label;
    optional<uint64_t> memory_limit_in_mib;
    params::TimeLimitsKind time_limits;
    optional<std::chrono::nanoseconds> fixed_time_limit;
    bool reset_scoring;
    bool look_for_new_tests;
};

Result<AddProblemAndReuploadProblemCommonParams, std::string>
validate_add_problem_and_reupload_problem_common_prams(Context& ctx) {
    STACK_UNWINDING_MARK;

    VALIDATE(ctx.request.form_fields, [](auto&& errors) { return Err{std::forward<decltype(errors)>(errors)}; },
        (ignore_simfile, params::ignore_simfile, REQUIRED)
        (package, params::package, FILE_REQUIRED)
    );
    VALIDATE(ctx.request.form_fields, [](auto&& errors) { return Err{std::forward<decltype(errors)>(errors)}; },
        (name, allow_blank_if(params::name, !ignore_simfile), REQUIRED)
        (label, allow_blank_if(params::label, !ignore_simfile), REQUIRED)
        (memory_limit_in_mib_str, allow_blank_if(params::memory_limit_in_mib_str, !ignore_simfile), REQUIRED)
    );

    optional<uint64_t> memory_limit_in_mib;
    if (!memory_limit_in_mib_str.empty()) {
        if (!is_digit(memory_limit_in_mib_str)) {
            return Err{std::string{"Invalid memory limit: not a number"}};
        }
        auto memory_limit_opt = str2num<uint32_t>(memory_limit_in_mib_str);
        if (!memory_limit_opt) {
            return Err{std::string{"Invalid memory limit: too big number"}};
        }
        memory_limit_in_mib = static_cast<uint64_t>(*memory_limit_opt);
    }

    // NOLINTNEXTLINE(bugprone-branch-clone)
    VALIDATE(ctx.request.form_fields, [](auto&& errors) { return Err{std::forward<decltype(errors)>(errors)}; },
        (time_limits, params::time_limits, REQUIRED_ENUM_CAPS(
            (KEEP_IF_POSSIBLE, !ignore_simfile)
            (RESET, true)
            (FIXED, true)
        ))
    );

    optional<std::chrono::nanoseconds> fixed_time_limit;
    // NOLINTNEXTLINE(bugprone-switch-missing-default-case)
    switch (time_limits) {
    case params::TimeLimitsKind::KEEP_IF_POSSIBLE:
    case params::TimeLimitsKind::RESET: break;
    case params::TimeLimitsKind::FIXED: {
        VALIDATE(ctx.request.form_fields, [](auto&& errors) { return Err{std::forward<decltype(errors)>(errors)}; },
            (fixed_time_limit_in_nsec_str, params::fixed_time_limit_in_nsec_str, REQUIRED)
        );
        if (!is_digit(fixed_time_limit_in_nsec_str)) {
            return Err{std::string{"Invalid fixed time limit: not a number"}};
        }
        using std::chrono::nanoseconds;
        auto fixed_time_limit_opt = str2num<nanoseconds::rep>(fixed_time_limit_in_nsec_str);
        if (!fixed_time_limit_opt) {
            return Err{std::string{"Invalid fixed time limit: too big number"}};
        }
        fixed_time_limit = nanoseconds{*fixed_time_limit_opt};
        if (fixed_time_limit < sim::MIN_TIME_LIMIT) {
            return Err{concat_tostr(
                "Invalid fixed time limit: cannot be smaller than ",
                to_string(sim::MIN_TIME_LIMIT),
                "s"
            )};
        }
        if (fixed_time_limit > sim::MAX_TIME_LIMIT) {
            return Err{concat_tostr(
                "Invalid fixed time limit: cannot be larger than ",
                to_string(sim::MAX_TIME_LIMIT),
                "s"
            )};
        }
    } break;
    }

    bool reset_scoring = false;
    bool look_for_new_tests = true;
    if (!ignore_simfile) {
        VALIDATE(ctx.request.form_fields, [](auto&& errors) { return Err{std::forward<decltype(errors)>(errors)}; },
            (reset_scoring_, params::reset_scoring, REQUIRED)
            (look_for_new_tests_, params::look_for_new_tests, REQUIRED)
        );
        reset_scoring = reset_scoring_;
        look_for_new_tests = look_for_new_tests_;
    }

    return Ok{AddProblemAndReuploadProblemCommonParams{
        .ignore_simfile = std::move(ignore_simfile),
        .package = std::move(package),
        .name = std::move(name),
        .label = std::move(label),
        .memory_limit_in_mib = std::move(memory_limit_in_mib),
        .time_limits = std::move(time_limits),
        .fixed_time_limit = std::move(fixed_time_limit),
        .reset_scoring = std::move(reset_scoring),
        .look_for_new_tests = std::move(look_for_new_tests),
    }};
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

Response
list_problems_with_visibility(Context& ctx, decltype(Problem::visibility) problem_visibility) {
    STACK_UNWINDING_MARK;
    return do_list_problems(ctx, FIRST_QUERY_LIMIT, problem_visibility, Condition("TRUE"));
}

Response list_problems_with_visibility_and_below_id(
    Context& ctx, decltype(Problem::visibility) problem_visibility, decltype(Problem::id) problem_id
) {
    STACK_UNWINDING_MARK;
    return do_list_problems(
        ctx, NEXT_QUERY_LIMIT, problem_visibility, Condition("p.id<?", problem_id)
    );
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

Response list_user_problems_with_visibility(
    Context& ctx, decltype(User::id) user_id, decltype(Problem::visibility) problem_visibility
) {
    STACK_UNWINDING_MARK;
    return do_list_user_problems(
        ctx, FIRST_QUERY_LIMIT, user_id, problem_visibility, Condition("TRUE")
    );
}

Response list_user_problems_with_visibility_and_below_id(
    Context& ctx,
    decltype(User::id) user_id,
    decltype(Problem::visibility) problem_visibility,
    decltype(Problem::id) problem_id
) {
    STACK_UNWINDING_MARK;
    return do_list_user_problems(
        ctx, NEXT_QUERY_LIMIT, user_id, problem_visibility, Condition("p.id<?", problem_id)
    );
}

Response view_problem(Context& ctx, decltype(Problem::id) problem_id) {
    STACK_UNWINDING_MARK;

    ProblemInfo p;
    p.id = problem_id;
    {
        auto stmt = ctx.mysql.execute(
            Select("visibility, owner_id").from("problems").where("id=?", problem_id)
        );
        stmt.res_bind(p.visibility, p.owner_id);
        if (not stmt.next()) {
            return ctx.response_404();
        }
    }
    auto caps = capabilities::problem(ctx.session, p.visibility, p.owner_id);
    if (not caps.view) {
        return ctx.response_403();
    }
    auto stmt = ctx.mysql.execute(
        Select("p.name, p.label, u.username, u.first_name, u.last_name, p.created_at, "
               "p.updated_at, s.full_status, p.simfile")
            .from("problems p")
            .left_join("users u")
            .on("u.id=p.owner_id")
            .left_join("submissions s")
            .on(Condition("s.problem_id=p.id") &&
                Condition(
                    "s.user_id=?", ctx.session ? optional{ctx.session->user_id} : std::nullopt
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
    p.append_to(caps, obj, public_tags, hidden_tags);
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

http::Response add(web_worker::Context& ctx) {
    STACK_UNWINDING_MARK;

    auto caps = capabilities::problems(ctx.session);
    if (not caps.add_problem) {
        return ctx.response_403();
    }

    VALIDATE(ctx.request.form_fields, ctx.response_400,
        (visibility, params::visibility, REQUIRED_ENUM_CAPS(
            (PRIVATE, caps.add_problem_with_visibility_private)
            (CONTEST_ONLY, caps.add_problem_with_visibility_contest_only)
            (PUBLIC, caps.add_problem_with_visibility_public)
        ))
    );

    auto res = validate_add_problem_and_reupload_problem_common_prams(ctx);
    if (res.is_err()) {
        return ctx.response_400(from_unsafe{std::move(res).unwrap_err()});
    }
    auto common_params = std::move(res).unwrap();

    auto file_id = sim::internal_files::new_internal_file_id(ctx.mysql);
    auto job_file_path = sim::internal_files::path_of(file_id);
    auto file_remover = FileRemover{job_file_path};
    // Make the uploaded package
    if (move(common_params.package.path, job_file_path)) {
        THROW("move()");
    }
    ctx.uncommited_files_removers.emplace_back(std::move(file_remover));

    auto job_type = Job::Type::ADD_PROBLEM;
    ctx.mysql.execute(InsertInto("jobs (created_at, creator, type, priority, status)")
                          .values(
                              "?, ?, ?, ?, ?",
                              utc_mysql_datetime(),
                              ctx.session->user_id,
                              job_type,
                              default_priority(job_type),
                              Job::Status::PENDING
                          ));
    auto job_id = ctx.old_mysql.insert_id();
    ctx.mysql.execute(
        InsertInto("add_problem_jobs (id, file_id, visibility, force_time_limits_reset, "
                   "ignore_simfile, name, label, memory_limit_in_mib, fixed_time_limit_in_ns, "
                   "reset_scoring, look_for_new_tests)")
            .values(
                "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?",
                job_id,
                file_id,
                visibility,
                common_params.time_limits == params::TimeLimitsKind::RESET,
                common_params.ignore_simfile,
                common_params.name,
                common_params.label,
                common_params.memory_limit_in_mib,
                common_params.fixed_time_limit ? optional{common_params.fixed_time_limit->count()}
                                               : std::nullopt,
                common_params.reset_scoring,
                common_params.look_for_new_tests
            )
    );
    ctx.notify_job_server_after_commit = true;

    json_str::Object obj;
    obj.prop_obj("job", [&](auto& obj) { obj.prop("id", job_id); });
    return ctx.response_json(std::move(obj).into_str());
}

Response reupload(Context& ctx, decltype(Problem::id) problem_id) {
    STACK_UNWINDING_MARK;

    auto caps_opt = get_problem_caps(ctx, problem_id);
    if (!caps_opt) {
        return ctx.response_404();
    }
    auto& caps = *caps_opt;
    if (!caps.reupload) {
        return ctx.response_403();
    }

    auto res = validate_add_problem_and_reupload_problem_common_prams(ctx);
    if (res.is_err()) {
        return ctx.response_400(from_unsafe{std::move(res).unwrap_err()});
    }
    auto common_params = std::move(res).unwrap();

    auto file_id = sim::internal_files::new_internal_file_id(ctx.mysql);
    auto job_file_path = sim::internal_files::path_of(file_id);
    auto file_remover = FileRemover{job_file_path};
    // Make the uploaded package
    if (move(common_params.package.path, job_file_path)) {
        THROW("move()");
    }
    ctx.uncommited_files_removers.emplace_back(std::move(file_remover));

    auto job_type = Job::Type::REUPLOAD_PROBLEM;
    ctx.mysql.execute(InsertInto("jobs (created_at, creator, type, priority, status, aux_id)")
                          .values(
                              "?, ?, ?, ?, ?, ?",
                              utc_mysql_datetime(),
                              ctx.session->user_id,
                              job_type,
                              default_priority(job_type),
                              Job::Status::PENDING,
                              problem_id
                          ));
    auto job_id = ctx.old_mysql.insert_id();
    ctx.mysql.execute(
        InsertInto(
            "reupload_problem_jobs (id, file_id, force_time_limits_reset, ignore_simfile, name, "
            "label, memory_limit_in_mib, fixed_time_limit_in_ns, reset_scoring, look_for_new_tests)"
        )
            .values(
                "?, ?, ?, ?, ?, ?, ?, ?, ?, ?",
                job_id,
                file_id,
                common_params.time_limits == params::TimeLimitsKind::RESET,
                common_params.ignore_simfile,
                common_params.name,
                common_params.label,
                common_params.memory_limit_in_mib,
                common_params.fixed_time_limit ? optional{common_params.fixed_time_limit->count()}
                                               : std::nullopt,
                common_params.reset_scoring,
                common_params.look_for_new_tests
            )
    );
    ctx.notify_job_server_after_commit = true;

    json_str::Object obj;
    obj.prop_obj("job", [&](auto& obj) { obj.prop("id", job_id); });
    return ctx.response_json(std::move(obj).into_str());
}

} // namespace web_server::problems::api

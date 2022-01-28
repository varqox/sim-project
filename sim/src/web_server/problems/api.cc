#include "src/web_server/problems/api.hh"
#include "sim/problem_tags/problem_tag.hh"
#include "sim/problems/problem.hh"
#include "sim/users/user.hh"
#include "simlib/config_file.hh"
#include "simlib/file_path.hh"
#include "simlib/json_str/json_str.hh"
#include "simlib/mysql/mysql.hh"
#include "simlib/string_view.hh"
#include "src/web_server/capabilities/problems.hh"
#include "src/web_server/http/response.hh"
#include "src/web_server/web_worker/context.hh"

#include <cstdint>

using sim::problem_tags::ProblemTag;
using sim::problems::Problem;
using sim::users::User;
using web_server::http::Response;
using web_server::web_worker::Context;

namespace {

struct ProblemInfo {
    decltype(Problem::id) id{};
    decltype(Problem::type) type{};
    decltype(Problem::name) name;
    decltype(Problem::label) label;
    mysql::Optional<decltype(Problem::owner_id)::value_type> owner_id;
    mysql::Optional<decltype(User::username)> owner_username;
    mysql::Optional<decltype(User::first_name)> owner_first_name;
    mysql::Optional<decltype(User::last_name)> owner_last_name;
    decltype(Problem::created_at) created_at;
    decltype(Problem::updated_at) updated_at;

private:
    decltype(ProblemTag::Id::name) tag_name;
    decltype(ProblemTag::is_hidden) tag_is_hidden;
    mysql::Statement tags_stmt;

public:
    explicit ProblemInfo(mysql::Connection& mysql)
    : tags_stmt{mysql.prepare("SELECT name, is_hidden FROM problem_tags WHERE problem_id=? "
                              "ORDER BY is_hidden, name")} {
        tags_stmt.bind_all(id);
        tags_stmt.res_bind_all(tag_name, tag_is_hidden);
    }

    ProblemInfo(const ProblemInfo&) = delete;
    ProblemInfo(ProblemInfo&&) = delete;
    ProblemInfo& operator=(const ProblemInfo&) = delete;
    ProblemInfo& operator=(ProblemInfo&&) = delete;
    ~ProblemInfo() = default;

    void append_to(decltype(Context::session)& session, json_str::ObjectBuilder& obj) {
        const auto caps = web_server::capabilities::problem(session, type, owner_id);
        assert(caps.view);
        obj.prop("id", id);
        obj.prop("type", type);
        obj.prop("name", name);
        obj.prop("label", label);
        if (owner_id and caps.view_owner) {
            obj.prop_obj("owner", [&](auto& obj) {
                obj.prop("id", owner_id);
                obj.prop("username", owner_username);
                obj.prop("first_name", owner_first_name);
                obj.prop("last_name", owner_last_name);
            });
        } else {
            obj.prop("owner", nullptr);
        }
        obj.prop("created_at", created_at);
        obj.prop("updated_at", updated_at);
        obj.prop_obj("tags", [&](auto& obj) {
            tags_stmt.execute();
            bool done = !tags_stmt.next();
            obj.prop_arr("public", [&](auto& arr) {
                while (!done and !tag_is_hidden) {
                    arr.val(tag_name);
                    done = !tags_stmt.next();
                }
            });
            obj.prop_arr("hidden", [&](auto& arr) {
                while (!done) {
                    arr.val(tag_name);
                    done = !tags_stmt.next();
                }
            });
        });
        obj.prop_obj("capabilities", [&](auto& obj) {
            obj.prop("view", caps.view);
            obj.prop("view_statement", caps.view_statement);
            obj.prop("view_public_tags", caps.view_public_tags);
            obj.prop("view_hidden_tags", caps.view_hidden_tags);
            obj.prop("view_owner", caps.view_owner);
            obj.prop("view_simfile", caps.view_simfile);
            obj.prop("download", caps.download);
            obj.prop("reupload", caps.reupload);
            obj.prop("rejudge_all_submissions", caps.rejudge_all_submissions);
            obj.prop("reset_time_limits", caps.reset_time_limits);
            obj.prop("delete", caps.delete_);
            obj.prop("merge_into_another_problem", caps.merge_into_another_problem);
            obj.prop("merge_other_problem_into_this_problem",
                    caps.merge_other_problem_into_this_problem);
        });
    }
};

template <class... BindArgs>
Response do_list(
        Context& ctx, uint32_t limit, StringView where_str = "", BindArgs&&... bind_args) {
    auto p = ProblemInfo(ctx.mysql);
    auto stmt = ctx.mysql.prepare("SELECT p.id, p.type, p.name, p.label, p.owner_id, u.username, "
                                  "u.first_name, u.last_name, p.created_at, p.updated_at FROM "
                                  "problems p LEFT JOIN users u ON u.id=p.owner_id ",
            where_str, " ORDER BY id DESC LIMIT ", limit);
    stmt.bind_and_execute(std::forward<BindArgs>(bind_args)...);
    stmt.res_bind_all(p.id, p.type, p.name, p.label, p.owner_id, p.owner_username,
            p.owner_first_name, p.owner_last_name, p.created_at, p.updated_at);

    json_str::Object obj;
    obj.prop("may_be_more", stmt.rows_num() == limit);
    obj.prop_arr("list", [&](auto& arr) {
        while (stmt.next()) {
            arr.val_obj([&](auto& obj) { p.append_to(ctx.session, obj); });
        }
    });
    return ctx.response_json(std::move(obj).into_str());
}

} // namespace

namespace web_server::problems::api {

constexpr inline uint32_t FIRST_QUERY_LIMIT = 64;
constexpr inline uint32_t NEXT_QUERY_LIMIT = 200;

Response list_problems(Context& ctx) {
    if (not capabilities::list_all_problems(ctx.session)) {
        return ctx.response_403();
    }
    return do_list(ctx, FIRST_QUERY_LIMIT);
}

Response list_problems_below_id(Context& ctx, decltype(Problem::id) problem_id) {
    if (not capabilities::list_all_problems(ctx.session)) {
        return ctx.response_403();
    }
    return do_list(ctx, NEXT_QUERY_LIMIT, "WHERE p.id<?", problem_id);
}

Response list_problems_of_user(Context& ctx, decltype(User::id) user_id) {
    if (not capabilities::list_problems_of_user(ctx.session, user_id)) {
        return ctx.response_403();
    }
    return do_list(ctx, FIRST_QUERY_LIMIT, "WHERE p.owner_id=?", user_id);
}

Response list_problems_of_user_below_id(
        Context& ctx, decltype(User::id) user_id, decltype(Problem::id) problem_id) {
    if (not capabilities::list_problems_of_user(ctx.session, user_id)) {
        return ctx.response_403();
    }
    return do_list(ctx, NEXT_QUERY_LIMIT, "WHERE p.owner_id=? AND p.id<?", user_id, problem_id);
}

Response list_problems_by_type(Context& ctx, decltype(Problem::type) problem_type) {
    if (not capabilities::list_problems_by_type(ctx.session, problem_type)) {
        return ctx.response_403();
    }
    return do_list(ctx, FIRST_QUERY_LIMIT, "WHERE p.type=?", problem_type);
}

Response list_problems_by_type_below_id(
        Context& ctx, decltype(Problem::type) problem_type, decltype(Problem::id) problem_id) {
    if (not capabilities::list_problems_by_type(ctx.session, problem_type)) {
        return ctx.response_403();
    }
    return do_list(
            ctx, NEXT_QUERY_LIMIT, "WHERE p.type=? AND p.id<?", problem_type, problem_id);
}

Response list_problems_of_user_by_type(
        Context& ctx, decltype(User::id) user_id, decltype(Problem::type) problem_type) {
    if (not capabilities::list_problems_of_user_by_type(ctx.session, user_id, problem_type)) {
        return ctx.response_403();
    }
    return do_list(
            ctx, FIRST_QUERY_LIMIT, "WHERE p.owner_id=? AND p.type=?", user_id, problem_type);
}

Response list_problems_of_user_by_type_below_id(Context& ctx, decltype(User::id) user_id,
        decltype(Problem::type) problem_type, decltype(Problem::id) problem_id) {
    if (not capabilities::list_problems_of_user_by_type(ctx.session, user_id, problem_type)) {
        return ctx.response_403();
    }
    return do_list(ctx, NEXT_QUERY_LIMIT, "WHERE p.owner_id=? AND p.type=? AND p.id<?", user_id,
            problem_type, problem_id);
}

Response view_problem(Context& ctx, decltype(Problem::id) problem_id) {
    auto p = ProblemInfo(ctx.mysql);
    p.id = problem_id;
    auto stmt = ctx.mysql.prepare("SELECT type, owner_id FROM problems WHERE id=?");
    stmt.bind_and_execute(problem_id);
    stmt.res_bind_all(p.type, p.owner_id);
    if (not stmt.next()) {
        return ctx.response_404();
    }
    auto caps = capabilities::problem(ctx.session, p.type, p.owner_id);
    if (not caps.view) {
        return ctx.response_403();
    }
    stmt = ctx.mysql.prepare("SELECT p.name, p.label, u.username, u.first_name, u.last_name, "
                             "p.created_at, p.updated_at, p.simfile FROM problems p LEFT JOIN users "
                             "u ON u.id=p.owner_id WHERE p.id=?");
    stmt.bind_and_execute(problem_id);
    decltype(Problem::simfile) simfile;
    stmt.res_bind_all(p.name, p.label, p.owner_username, p.owner_first_name, p.owner_last_name,
            p.created_at, p.updated_at, simfile);
    throw_assert(stmt.next());

    json_str::Object obj;
    p.append_to(ctx.session, obj);
    if (caps.view_simfile) {
        obj.prop("simfile", simfile);
        ConfigFile cf;
        cf.add_vars("memory_limit");
        cf.load_config_from_string(simfile.to_string());
        obj.prop("default_memory_limit", cf.get_var("memory_limit").as<uint64_t>().value());
        // TODO: default_memory_limit should be a separate column in the problems table
    }
    return ctx.response_json(std::move(obj).into_str());
}

} // namespace web_server::problems::api

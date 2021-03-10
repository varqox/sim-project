#include "src/web_server/capabilities/problems.hh"
#include "sim/users/user.hh"

using sim::users::User;

namespace web_server::capabilities {

Problems problems_for(const decltype(web_worker::Context::session)& session) noexcept {
    bool is_admin = session and session->user_type == User::Type::ADMIN;
    bool is_teacher = session and session->user_type == User::Type::TEACHER;
    return Problems{
        .web_ui_view = true,
        .view_my = session.has_value(),
        .view_all = is_admin,
        .view_with_type_contest_only = is_admin or is_teacher,
        .view_with_type_public = true,
        .select_by_owner = is_admin or is_teacher,
        .add_with_type_private = is_admin or is_teacher,
        .add_with_type_contest_only = is_admin or is_teacher,
        .add_with_type_public = is_admin,
    };
}

} // namespace web_server::capabilities

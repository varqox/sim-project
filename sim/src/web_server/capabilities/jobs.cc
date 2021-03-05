#include "src/web_server/capabilities/jobs.hh"

using sim::users::User;

namespace web_server::capabilities {

Jobs jobs_for(const decltype(web_worker::Context::session)& session) noexcept {
    bool is_admin = session and session->user_type == User::Type::ADMIN;
    return Jobs{
        .view = is_admin,
        .view_all = is_admin,
    };
}

} // namespace web_server::capabilities

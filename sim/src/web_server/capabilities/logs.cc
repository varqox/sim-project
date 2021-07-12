#include "src/web_server/capabilities/logs.hh"
#include "sim/users/user.hh"

using sim::users::User;

namespace web_server::capabilities {

Logs logs_for(const decltype(web_worker::Context::session)& session) noexcept {
    return Logs{
        .view = session and session->user_type == User::Type::ADMIN,
    };
}

} // namespace web_server::capabilities

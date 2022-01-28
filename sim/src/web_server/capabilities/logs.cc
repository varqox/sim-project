#include "src/web_server/capabilities/logs.hh"
#include "sim/users/user.hh"
#include "src/web_server/capabilities/utils.hh"

namespace web_server::capabilities {

Logs logs_for(const decltype(web_worker::Context::session)& session) noexcept {
    return Logs{
            .view = is_admin(session),
    };
}

} // namespace web_server::capabilities

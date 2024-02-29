#include "jobs.hh"
#include "utils.hh"

namespace web_server::capabilities {

Jobs jobs_for(const decltype(web_worker::Context::session)& session) noexcept {
    return Jobs{
        .web_ui_view = is_admin(session),
        .view_all = is_admin(session),
    };
}

} // namespace web_server::capabilities

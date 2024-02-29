#include "submissions.hh"
#include "utils.hh"

#include <sim/users/user.hh>

namespace web_server::capabilities {

Submissions submissions_for(const decltype(web_worker::Context::session)& session) noexcept {
    return Submissions{
        .web_ui_view = session.has_value(),
        .view_my = session.has_value(),
        .view_all = is_admin(session),
    };
}

} // namespace web_server::capabilities

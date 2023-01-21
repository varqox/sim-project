#pragma once

#include "http/response.hh"
#include "web_worker/context.hh"
#include <sim/users/user.hh>
#include <simlib/string_view.hh>

namespace web_server {

struct UiTemplateParams {
    StringView title;
    const decltype(web_worker::Context::session)& session;
    StringView notifications;
};

void begin_ui_template(http::Response& resp, UiTemplateParams params);
void end_ui_template(http::Response& resp);
std::string sim_template_params(const decltype(web_worker::Context::session)& session);

} // namespace web_server

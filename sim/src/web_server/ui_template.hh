#pragma once

#include "sim/users/user.hh"
#include "simlib/string_view.hh"
#include "src/web_server/http/response.hh"
#include "src/web_server/web_worker/context.hh"

namespace web_server {

struct UiTemplateParams {
    StringView title;
    const decltype(web_worker::Context::session)& session;
    StringView notifications;
};

void begin_ui_template(http::Response& resp, UiTemplateParams params);
void end_ui_template(http::Response& resp);

} // namespace web_server

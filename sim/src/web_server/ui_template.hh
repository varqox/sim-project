#pragma once

#include "sim/sessions/session.hh"
#include "sim/users/user.hh"
#include "simlib/string_view.hh"
#include "src/web_server/http/response.hh"

namespace web_server {

struct UiTemplateParams {
    StringView title;
    StringView styles;
    bool show_users;
    bool show_submissions;
    bool show_job_queue;
    bool show_logs;
    decltype(sim::sessions::Session::user_id)* session_user_id;
    decltype(sim::users::User::type)* session_user_type;
    decltype(sim::users::User::username)* session_username;
    StringView notifications;
};

void begin_ui_template(http::Response& resp, UiTemplateParams params);
void end_ui_template(http::Response& resp);

} // namespace web_server

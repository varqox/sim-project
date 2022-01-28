#pragma once

#include "sim/users/user.hh"
#include "src/web_server/web_worker/context.hh"

namespace web_server::capabilities {

constexpr bool is_admin(const decltype(web_worker::Context::session)& session) noexcept {
    return session and session->user_type == sim::users::User::Type::ADMIN;
}

constexpr bool is_teacher(const decltype(web_worker::Context::session)& session) noexcept {
    return session and session->user_type == sim::users::User::Type::TEACHER;
}

constexpr bool is_self(const decltype(web_worker::Context::session)& session,
        decltype(sim::users::User::id) user_id) noexcept {
    return session and session->user_id == user_id;
}

} // namespace web_server::capabilities

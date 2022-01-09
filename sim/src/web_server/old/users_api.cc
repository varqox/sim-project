#include "sim/is_username.hh"
#include "sim/jobs/utils.hh"
#include "sim/users/user.hh"
#include "simlib/random.hh"
#include "simlib/sha.hh"
#include "simlib/string_transform.hh"
#include "simlib/time.hh"
#include "simlib/utilities.hh"
#include "src/web_server/old/sim.hh"

#include <cstdint>
#include <limits>
#include <type_traits>

using sim::jobs::Job;
using sim::users::User;
using std::string;

namespace web_server::old {

void Sim::api_user() {
    STACK_UNWINDING_MARK;

    if (not session.has_value()) {
        return api_error403();
    }

    StringView next_arg = url_args.extract_next_arg();
    if (not is_digit(next_arg) or request.method != http::Request::POST) {
        return api_error400();
    }

    auto uid_opt = str2num<decltype(users_uid)>(next_arg);
    if (not uid_opt) {
        return api_error404();
    }
    users_uid = *uid_opt;
    users_perms = users_get_permissions(users_uid);
    if (users_perms == UserPermissions::NONE) {
        return api_error404();
    }
    return api_error404();
}

} // namespace web_server::old

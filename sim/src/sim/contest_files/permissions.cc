#include "sim/contest_files/permissions.hh"

namespace sim::contest_files {

OverallPermissions get_overall_permissions(contests::Permissions cperms) noexcept {
    STACK_UNWINDING_MARK;
    using PERM = OverallPermissions;

    if (uint(cperms & contests::Permissions::ADMIN)) {
        return PERM::ADD | PERM::VIEW_CREATOR;
    }

    return PERM::NONE;
}

Permissions get_permissions(contests::Permissions cperms) noexcept {
    STACK_UNWINDING_MARK;
    using PERM = Permissions;
    using CPERM = contests::Permissions;

    if (uint(cperms & CPERM::ADMIN)) {
        return PERM::VIEW | PERM::DOWNLOAD | PERM::EDIT | PERM::DELETE;
    }

    if (uint(cperms & CPERM::VIEW)) {
        return PERM::VIEW | PERM::DOWNLOAD;
    }

    return PERM::NONE;
}

} // namespace sim::contest_files

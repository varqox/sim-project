#include <sim/contest_file_permissions.hh>

namespace sim::contest_file {

OverallPermissions
get_overall_permissions(contest::Permissions cperms) noexcept {
	STACK_UNWINDING_MARK;
	using PERM = OverallPermissions;

	if (uint(cperms & contest::Permissions::ADMIN)) {
		return PERM::ADD | PERM::VIEW_CREATOR;
	}

	return PERM::NONE;
}

Permissions get_permissions(contest::Permissions cperms) noexcept {
	STACK_UNWINDING_MARK;
	using PERM = Permissions;
	using CPERM = sim::contest::Permissions;

	if (uint(cperms & CPERM::ADMIN)) {
		return PERM::VIEW | PERM::DOWNLOAD | PERM::EDIT | PERM::DELETE;
	}

	if (uint(cperms & CPERM::VIEW)) {
		return PERM::VIEW | PERM::DOWNLOAD;
	}

	return PERM::NONE;
}

} // namespace sim::contest_file

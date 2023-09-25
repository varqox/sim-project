#pragma once

#include "request.hh"

#include <simlib/array_vec.hh>
#include <simlib/deserialize.hh>

namespace sandbox::supervisor::request {

void deserialize(deserialize::Reader& reader, ArrayVec<int, 253>& fds, Request& req);

} // namespace sandbox::supervisor::request

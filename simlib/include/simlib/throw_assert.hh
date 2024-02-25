#pragma once

#include <simlib/concat_tostr.hh>
#include <simlib/macros/stringify.hh>
#include <stdexcept>

#define throw_assert(expr)                               \
    ((expr) ? (void)0                                    \
            : throw std::runtime_error(concat_tostr(     \
                  __FILE__ ":" STRINGIFY(__LINE__) ": ", \
                  __PRETTY_FUNCTION__,                   \
                  ": Assertion `" #expr "` failed."      \
              )))

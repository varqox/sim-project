#pragma once

#include <simlib/concat_tostr.hh>
#include <simlib/macros/stringify.hh>
#include <stdexcept>

// Very useful - includes exception origin
#define THROW(...)                                                                     \
    throw std::runtime_error(                                                          \
        concat_tostr(__VA_ARGS__, " (thrown at " __FILE__ ":" STRINGIFY(__LINE__) ")") \
    )

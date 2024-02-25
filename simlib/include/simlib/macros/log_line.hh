#pragma once

#include <simlib/macros/stringify.hh>

#define LOG_LINE stdlog(__FILE__ ":" STRINGIFY(__LINE__))

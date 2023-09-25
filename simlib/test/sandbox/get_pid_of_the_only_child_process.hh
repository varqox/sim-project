#pragma once

#include <cctype>
#include <simlib/file_contents.hh>
#include <simlib/string_transform.hh>
#include <simlib/string_view.hh>
#include <simlib/throw_assert.hh>
#include <unistd.h>

inline pid_t get_pid_of_the_only_child_process() {
    auto children_pids = get_file_contents(concat_tostr("/proc/self/task/", gettid(), "/children"));
    auto child_pid = str2num<pid_t>(StringView{children_pids}.without_trailing(&isspace));
    throw_assert(child_pid.has_value());
    return *child_pid;
}

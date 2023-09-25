#pragma once

#include "communication/stack_buff_writer.hh"

#include <cstddef>
#include <simlib/concat_common.hh>
#include <simlib/errmsg.hh>
#include <unistd.h>
#include <utility>

namespace sandbox {

template <size_t BUFF_SIZE = 128, class... Args>
[[noreturn]] void do_die_with_msg(int error_fd, Args&&... msg) noexcept {
    communication::StackBuffWriter<BUFF_SIZE> writer(error_fd, [](int /*unused*/) { _exit(1); });
    auto append = [&writer](auto&& str) { writer.write(data(str), string_length(str)); };
    (append(stringify(std::forward<Args>(msg))), ...);
    writer.flush();
    _exit(1);
}

template <size_t BUFF_SIZE = 128, class... Args>
[[noreturn]] void do_die_with_error(int error_fd, Args&&... msg) noexcept {
    do_die_with_msg<BUFF_SIZE>(error_fd, std::forward<Args>(msg)..., errmsg());
}

} // namespace sandbox

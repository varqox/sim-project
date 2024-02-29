#pragma once

#include <array>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <exception>
#include <simlib/macros/likely.hh>
#include <unistd.h>

namespace sandbox::communication {

template <size_t BUFF_SIZE = 128>
class StackBuffWriter {
    std::array<char, BUFF_SIZE> buff;
    size_t buff_len = 0;
    int fd;
    void (*handle_write_error)(int errnum);

    void do_write_exact(const char* data, size_t len) {
        size_t written = 0;
        while (written < len) {
            auto rc = ::write(fd, data + written, len - written);
            if (LIKELY(rc > 0)) {
                written += rc;
                continue;
            }
            if (UNLIKELY(rc == 0)) {
                handle_write_error(0);
                std::terminate(); // handler should not return
            }
            assert(rc < 0);
            if (errno == EINTR) {
                continue;
            }
            handle_write_error(errno);
            std::terminate(); // handler should not return
        }
    }

public:
    StackBuffWriter(int fd, void (*handle_write_error)(int errnum)) noexcept
    : fd{fd}
    , handle_write_error(handle_write_error) {}

    StackBuffWriter(const StackBuffWriter&) = delete;
    StackBuffWriter(StackBuffWriter&&) = delete;
    StackBuffWriter& operator=(const StackBuffWriter&) = delete;
    StackBuffWriter& operator=(StackBuffWriter&&) = delete;

    void write(const char* data, size_t len) {
        if (buff_len + len <= buff.size()) {
            std::memcpy(buff.data() + buff_len, data, len);
            buff_len += len;
            return;
        }

        flush();
        assert(buff_len == 0);
        if (len > buff.size() / 2) {
            do_write_exact(data, len);
            return;
        }
        std::memcpy(buff.data(), data, len);
        buff_len = len;
    }

    void flush() {
        do_write_exact(buff.data(), buff_len);
        buff_len = 0;
    }

    // Does not flush on destruction, you should call flush instead; useful in case of stack
    // unwinding
    ~StackBuffWriter() = default;
};

} // namespace sandbox::communication

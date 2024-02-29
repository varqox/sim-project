#pragma once

#include <simlib/file_descriptor.hh>
#include <simlib/throw_assert.hh>
#include <sys/socket.h>

struct UnixSocketpair {
    FileDescriptor our_end;
    FileDescriptor other_end;
};

inline UnixSocketpair unix_socketpair(int type) {
    int sock_fds[2];
    throw_assert(socketpair(AF_UNIX, type, 0, sock_fds) == 0);
    return {
        .our_end = FileDescriptor{sock_fds[0]},
        .other_end = FileDescriptor{sock_fds[1]},
    };
}

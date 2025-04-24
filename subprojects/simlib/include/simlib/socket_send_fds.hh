#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <sys/socket.h>
#include <sys/uio.h>

template <size_t MAX_FDS_LEN>
ssize_t send_fds(
    int sock_fd, const void* data, size_t data_len, int flags, const int* fds, size_t fds_len
) noexcept {
    assert(fds_len <= MAX_FDS_LEN);
    struct iovec iov = {
        .iov_base = const_cast<char*>(static_cast<const char*>(data)),
        .iov_len = data_len,
    };

    // Allocate ancillary data buffer with struct cmsghdr alignment
    alignas(struct cmsghdr) char buf[CMSG_SPACE(MAX_FDS_LEN * sizeof(*fds))];
    std::memset(buf, 0, sizeof(buf)); // Needed for CMSG_NXTHDR() to work correctly.
    struct msghdr msg = {
        .msg_name = nullptr,
        .msg_namelen = 0,
        .msg_iov = &iov,
        .msg_iovlen = 1,
        .msg_control = buf,
        .msg_controllen = CMSG_SPACE(fds_len * sizeof(*fds)),
        .msg_flags = 0, // ignored, but set anyway
    };
    auto cmsg = CMSG_FIRSTHDR(&msg);
    *cmsg = {
        .cmsg_len = CMSG_LEN(fds_len * sizeof(*fds)),
        .cmsg_level = SOL_SOCKET,
        .cmsg_type = SCM_RIGHTS,
    };
    std::memcpy(CMSG_DATA(cmsg), fds, fds_len * sizeof(*fds));

    ssize_t res;
    do {
        res = sendmsg(sock_fd, &msg, flags);
    } while (res == -1 && errno == EINTR);

    return res;
}

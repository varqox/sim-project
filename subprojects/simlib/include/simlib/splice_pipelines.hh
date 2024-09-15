#pragma once

#include <array>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <fcntl.h>
#include <optional>
#include <poll.h>
#include <simlib/errmsg.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/macros/throw.hh>

struct Pipeline {
    FileDescriptor readable_fd;
    FileDescriptor writable_fd;
    std::optional<uint64_t> transferred_data_limit_in_bytes = std::nullopt;
};

template <size_t N>
struct SplicePipelinesRes {
    struct BrokenPipeline {
        bool readable = false;
        bool writable = false;
    };

    std::array<BrokenPipeline, N> first_broken_pipelines;
    std::array<bool, N> transferred_data_limit_exceeded;
};

template <size_t N>
SplicePipelinesRes<N> splice_pipelines(Pipeline (&&pipelines)[N]) {
    // Ignore potential SIGPIPE
    sigset_t sigpipe_set;
    sigset_t old_sigmask;
    sigemptyset(&sigpipe_set);
    sigaddset(&sigpipe_set, SIGPIPE);
    if (pthread_sigmask(SIG_BLOCK, &sigpipe_set, &old_sigmask)) {
        THROW("pthread_sigmask()");
    }
    bool encoutered_sigpipe = false;

    std::array<pollfd, N * 2> pfds;

    struct PipelinePfds {
        pollfd& readable; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
        pollfd& writable; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    };

    auto pipeline_pfds = [&pfds](size_t i) {
        return PipelinePfds{
            .readable = pfds[i << 1],
            .writable = pfds[i << 1 | 1],
        };
    };
    for (size_t i = 0; i < N; ++i) {
        auto [pfd_rd, pfd_wr] = pipeline_pfds(i);
        pfd_rd = {
            .fd = pipelines[i].readable_fd,
            .events = POLLIN,
            .revents = 0,
        };
        pfd_wr = {
            .fd = pipelines[i].writable_fd,
            .events = POLLOUT,
            .revents = 0,
        };
    }
    // Make all fds non-blocking
    for (const auto& pfd : pfds) {
        if (fcntl(pfd.fd, F_SETFL, O_NONBLOCK)) {
            THROW("fcntl()", errmsg());
        }
    }

    struct PipelineState {
        bool closed = false;

        struct RdState {
            bool hup = false;
            bool in = false;
        } rd_state = {};

        struct WrState {
            bool err = false;
            bool out = false;
        } wr_state = {};
    };

    std::array<PipelineState, N> states = {};

    bool found_first_broken_pipelines = false;
    auto res = SplicePipelinesRes<N>{
        .first_broken_pipelines = {},
        .transferred_data_limit_exceeded = {},
    };
    size_t open_pipelines = N;
    while (open_pipelines > 0) {
        int rc = poll(pfds.data(), pfds.size(), -1);
        if (rc == 0 || (rc == -1 && errno == EINTR)) {
            continue;
        }
        if (rc == -1) {
            THROW("poll()", errmsg());
        }

        // Look for the first broken pipeline
        if (!found_first_broken_pipelines) {
            for (size_t i = 0; i < N; ++i) {
                auto [pfd_rd, pfd_wr] = pipeline_pfds(i);
                if (pfd_rd.revents & POLLHUP) {
                    res.first_broken_pipelines[i].readable = true;
                    found_first_broken_pipelines = true;
                }
                if (pfd_wr.revents & POLLERR) {
                    res.first_broken_pipelines[i].writable = true;
                    found_first_broken_pipelines = true;
                }
            }
        }

        for (size_t i = 0; i < N; ++i) {
            auto& state = states[i];
            if (state.closed) {
                continue;
            }

            auto ppfd = pipeline_pfds(i);
            state.rd_state.hup |= static_cast<bool>(ppfd.readable.revents & POLLHUP);
            state.rd_state.in |= static_cast<bool>(ppfd.readable.revents & POLLIN);
            if (state.rd_state.hup) {
                ppfd.readable.fd = -1; // stop listening on this file descriptor
            } else if (state.rd_state.in) {
                ppfd.readable.events = 0; // still listen for POLLHUP
            }

            state.wr_state.err |= static_cast<bool>(ppfd.writable.revents & POLLERR);
            state.wr_state.out |= static_cast<bool>(ppfd.writable.revents & POLLOUT);
            if (state.wr_state.err) {
                ppfd.writable.fd = -1; // stop listening on this file descriptor
            } else if (state.wr_state.out) {
                ppfd.writable.events = 0; // still listen for POLLERR
            }

            auto& pipeline = pipelines[i];
            auto close_pipeline = [&] {
                if (pipeline.readable_fd.close()) {
                    THROW("close()", errmsg());
                }
                ppfd.readable.fd = -1; // stop listening on this file descriptor
                if (pipeline.writable_fd.close()) {
                    THROW("close()", errmsg());
                }
                ppfd.writable.fd = -1; // stop listening on this file descriptor
                state.closed = true;
                --open_pipelines;
            };

            if (state.wr_state.err) {
                // We cannot write more data
                close_pipeline();
                continue;
            }
            if (state.rd_state.hup && !state.rd_state.in) {
                // No more data to read
                close_pipeline();
                continue;
            }

            if (state.rd_state.in && state.wr_state.out) {
                if (pipeline.transferred_data_limit_in_bytes == 0) {
                    res.transferred_data_limit_exceeded[i] = true;
                    close_pipeline();
                    continue;
                }
                auto to_write = pipeline.transferred_data_limit_in_bytes.value_or(1 << 30);
                auto sent = splice(
                    pipeline.readable_fd,
                    nullptr,
                    pipeline.writable_fd,
                    nullptr,
                    to_write,
                    SPLICE_F_NONBLOCK | SPLICE_F_MOVE
                );
                if (sent < 0) {
                    if (errno == EPIPE) { // write end was closed after poll() returned
                        sent = 0;
                        encoutered_sigpipe = true;
                    } else {
                        THROW("splice()", errmsg());
                    }
                }
                if (sent == 0) {
                    // No more data
                    close_pipeline();
                    continue;
                }
                if (pipeline.transferred_data_limit_in_bytes) {
                    *pipeline.transferred_data_limit_in_bytes -= sent;
                }
                state.rd_state.in = false;
                ppfd.readable.fd =
                    pipeline.readable_fd; // in case state.rd_state.hup == true, we still
                                          // need to check if there is more input to read
                ppfd.readable.events = POLLIN;
                state.wr_state.out = false;
                assert(ppfd.writable.fd >= 0);
                ppfd.writable.events = POLLOUT;
            }
        }
    }

    // Consume potential SIGPIPE (it may not be generated if signal disposition is set to SIG_IGN)
    if (encoutered_sigpipe) {
        struct timespec timeout = {.tv_sec = 0, .tv_nsec = 0};
        if (sigtimedwait(&sigpipe_set, nullptr, &timeout) == -1 && errno != EAGAIN) {
            THROW("sigtimedwait()");
        }
    }

    // Restore the previous thread's sigmask
    if (pthread_sigmask(SIG_SETMASK, &old_sigmask, nullptr)) {
        THROW("pthread_sigmask()");
    }

    return res;
}

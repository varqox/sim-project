#pragma once

#include <gtest/gtest.h>
#include <poll.h>
#include <simlib/overloaded.hh>
#include <variant>

struct PollTimeout {};

struct PollReady {
    decltype(pollfd::revents) revents;
};

inline void check_poll(
    int fd, decltype(pollfd::events) events, std::variant<PollTimeout, PollReady> expected_res
) {
    pollfd pfd = {
        .fd = fd,
        .events = events,
        .revents = 0,
    };
    std::visit(
        overloaded{
            [&pfd](const PollTimeout&) { ASSERT_EQ(poll(&pfd, 1, 0), 0); },
            [&pfd](const PollReady& pr) {
                ASSERT_EQ(poll(&pfd, 1, 10000), 1);
                ASSERT_EQ(pfd.revents, pr.revents);
            }
        },
        expected_res
    );
}

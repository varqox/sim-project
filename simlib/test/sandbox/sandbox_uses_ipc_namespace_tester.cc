#include <fcntl.h>
#include <mqueue.h>
#include <simlib/throw_assert.hh>

int main(int argc, char** argv) {
    throw_assert(argc == 2);
    throw_assert(mq_open(argv[1], O_RDWR | O_CLOEXEC) == -1 && errno == ENOENT);
}

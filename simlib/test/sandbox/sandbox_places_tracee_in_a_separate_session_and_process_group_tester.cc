#include <simlib/throw_assert.hh>
#include <unistd.h>

int main() {
    pid_t pid = getpid();
    throw_assert(getpgid(0) == pid);
    throw_assert(getsid(0) == pid);
}

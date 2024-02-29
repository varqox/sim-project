#include <simlib/throw_assert.hh>
#include <sys/prctl.h>

int main() { throw_assert(prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0) == 1); }

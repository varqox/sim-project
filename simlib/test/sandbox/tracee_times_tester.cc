#include <ctime>
#include <simlib/errmsg.hh>
#include <simlib/macros/throw.hh>
#include <simlib/timespec_arithmetic.hh>

timespec get_cpu_time() {
    timespec ts;
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts)) {
        THROW("clock_gettime()", errmsg());
    }
    return ts;
}

int main() {
    timespec beg = get_cpu_time();
    for (;;) {
        if (get_cpu_time() - beg >= timespec{.tv_sec = 0, .tv_nsec = 50'000'000}) {
            break;
        }
    }
}

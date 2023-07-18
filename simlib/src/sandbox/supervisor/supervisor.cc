#include <unistd.h>

int main() noexcept {
    for (;;) {
        pause();
    }
    return 0;
}

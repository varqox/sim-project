#include <array>
#include <simlib/noexcept_concat.hh>
#include <simlib/throw_assert.hh>
#include <unistd.h>

int main() {
    std::array<char, 4096> buff;
    throw_assert(gethostname(buff.data(), buff.size()) == 0 && buff[0] == '\0');
    throw_assert(getdomainname(buff.data(), buff.size()) == 0 && buff[0] == '\0');
}

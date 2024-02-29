#include <array>
#include <simlib/throw_assert.hh>
#include <string_view>
#include <unistd.h>

int main(int argc, char** argv) {
    throw_assert(argc == 2);
    std::array<char, 32> time_ns_id;
    auto time_ns_id_len = readlink("/proc/self/ns/time", time_ns_id.data(), time_ns_id.size());
    throw_assert(time_ns_id_len > 0 && static_cast<size_t>(time_ns_id_len) < time_ns_id.size());

    throw_assert(std::string_view(time_ns_id.data(), time_ns_id_len) != std::string_view{argv[1]});
}

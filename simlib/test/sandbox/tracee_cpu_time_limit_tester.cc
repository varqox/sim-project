#include <cstddef>
#include <simlib/string_transform.hh>
#include <simlib/throw_assert.hh>
#include <thread>

int main(int argc, char** argv) {
    throw_assert(argc == 2);
    auto thread_num = str2num<size_t>(argv[1]).value();

    for (size_t i = 1; i < thread_num; ++i) {
        std::thread{[] {
            for (;;) {
            }
        }}.detach();
    }
    for (;;) {
    }
}

#include <cstdint>
#include <cstring>
#include <simlib/errmsg.hh>
#include <simlib/macros/throw.hh>
#include <simlib/string_transform.hh>
#include <simlib/throw_assert.hh>
#include <sys/mman.h>

int main(int argc, char** argv) {
    throw_assert(2 <= argc && argc <= 3);
    auto mem_mib = str2num<uint32_t>(argv[1]).value(); // NOLINT(bugprone-unchecked-optional-access)
    bool fill_mem = false;
    if (argc == 3) {
        throw_assert(StringView{argv[2]} == "fill");
        fill_mem = true;
    }

    auto mem_size = static_cast<uint64_t>(mem_mib) << 20;
    auto ptr = mmap(nullptr, mem_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) { // NOLINT(performance-no-int-to-ptr)
        THROW("mmap()", errmsg());
    }

    if (fill_mem) {
        std::memset(ptr, 1, mem_size);
    }
}

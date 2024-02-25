#include <array>
#include <cerrno>
#include <simlib/recursive_readlink.hh>
#include <string>
#include <unistd.h>

int recursive_readlink(std::string path, std::string& res) {
    std::array<char, 4096> buff;
    for (int i = 0; i < 40; ++i) {
        auto rc = readlink(path.c_str(), buff.data(), buff.size());
        if (rc == -1 && errno == EINVAL) {
            res = path;
            return 0;
        }
        if (rc < 0) {
            return -1;
        }
        if (static_cast<size_t>(rc) == buff.size()) {
            errno = EOVERFLOW;
            return -1;
        }
        buff[rc] = '\0';
        if (buff[0] == '/') {
            path.clear();
        } else {
            // Remove the last component
            while (!path.empty() && path.back() != '/') {
                path.pop_back();
            }
        }
        path.append(buff.data(), rc);
    }
    errno = ELOOP;
    return -1;
}

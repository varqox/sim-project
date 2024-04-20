#include <sim/random.hh>
#include <simlib/random.hh>
#include <string_view>

namespace sim {

std::string generate_random_token(size_t length) {
    static constexpr auto chars =
        std::string_view{"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"};
    std::string res(length, '0');
    for (char& c : res) {
        c = chars[get_random<size_t>(0, chars.size() - 1)];
    }
    return res;
}

} // namespace sim

#include <sim/users/user.hh>
#include <simlib/concat.hh>
#include <simlib/from_unsafe.hh>
#include <simlib/random.hh>
#include <simlib/sha.hh>
#include <simlib/string_compare.hh>
#include <simlib/string_transform.hh>

namespace sim::users {

std::pair<decltype(User::password_salt), decltype(User::password_hash)>
salt_and_hash_password(std::string_view password) {
    constexpr auto salt_len = decltype(User::password_salt)::len;
    InplaceBuff<salt_len / 2> salt_bin(salt_len / 2);
    fill_randomly(salt_bin.data(), salt_bin.size);
    decltype(User::password_salt) salt{to_hex<salt_len>(salt_bin).to_string()};
    auto salted_password = concat(salt, password);
    decltype(User::password_hash) hash{sha3_512(salted_password).to_string()};
    return {std::move(salt), std::move(hash)};
}

bool password_matches(
    std::string_view password, std::string_view password_salt, std::string_view password_hash
) noexcept {
    auto salted_password = concat(password_salt, password);
    return slow_equal(from_unsafe{sha3_512(salted_password)}, password_hash);
}

} // namespace sim::users

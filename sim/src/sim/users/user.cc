#include <sim/users/user.hh>
#include <simlib/random.hh>
#include <simlib/sha.hh>
#include <simlib/string_compare.hh>

namespace sim::users {

std::pair<decltype(User::password_salt), decltype(User::password_hash)>
salt_and_hash_password(StringView password) {
    constexpr auto salt_len = decltype(User::password_salt)::max_len;
    InplaceBuff<salt_len / 2> salt_bin(salt_len / 2);
    fill_randomly(salt_bin.data(), salt_bin.size);
    decltype(User::password_salt) salt{to_hex<salt_len>(salt_bin)};
    auto salted_password = concat(salt, password);
    decltype(User::password_hash) hash{sha3_512(salted_password)};
    return {std::move(salt), std::move(hash)};
}

bool password_matches(
    StringView password, StringView password_salt, StringView password_hash
) noexcept {
    auto salted_password = concat(password_salt, password);
    return slow_equal(intentional_unsafe_string_view(sha3_512(salted_password)), password_hash);
}

} // namespace sim::users

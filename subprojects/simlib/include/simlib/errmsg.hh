#pragma once

#include <cstring>
#include <simlib/static_cstring_buff.hh>
#include <simlib/to_string.hh>

inline auto errmsg(int errnum) noexcept {
    constexpr auto s1 = StaticCStringBuff{" - "};
    constexpr auto bytes_for_error_description =
        64; // At the time of writing, longest error description is 50 bytes in
            // size (including null terminator)
    constexpr auto s2 = StaticCStringBuff{" (os error "};
    auto errnum_str = to_string(errnum);
    constexpr auto s3 = StaticCStringBuff{")"};
    StaticCStringBuff<
        s1.size() + bytes_for_error_description + s2.size() + decltype(errnum_str)::max_size() +
        s3.size()>
        res;
    size_t pos = 0;
    auto append = [&res, &pos](const auto& s) noexcept {
        for (auto c : s) {
            res[pos++] = c;
        }
    };
    // Prefix
    append(s1);
    // Error description
    const char* errstr = strerror_r(errnum, res.data() + pos, decltype(res)::max_size() - pos);
    if (errstr == res.data() + pos) {
        while (pos < decltype(res)::max_size() and res[pos] != '\0') {
            ++pos;
        }
    } else {
        if (errstr == nullptr) {
            errstr = "Unknown error";
        }
        while (pos < decltype(res)::max_size() and *errstr != '\0') {
            res[pos++] = *(errstr++);
        }
    }
    // Ensure the suffix will fit
    pos = std::min(pos, decltype(res)::max_size() - s2.size() - errnum_str.size() - s3.size());
    // Suffix
    append(s2);
    append(errnum_str);
    append(s3);
    // Null terminator
    res[pos] = '\0';
    res.len_ = pos;
    return res;
}

inline auto errmsg() noexcept { return errmsg(errno); }

#include <simlib/escape_bytes_to_utf8_str.hh>
#include <simlib/string_transform.hh>
#include <string>
#include <string_view>

using std::string;
using std::string_view;

string escape_bytes_to_utf8_str(
    const string_view& prefix, const string_view& bytes, const string_view& suffix
) {
    static constexpr auto is_a_continuation_byte = [](unsigned char c) {
        return (c & 0b11000000) == 0b10000000;
    };
    static constexpr auto starts_2_byte_sequence = [](unsigned char c) {
        return (c & 0b11100000) == 0b11000000;
    };
    static constexpr auto starts_3_byte_sequence = [](unsigned char c) {
        return (c & 0b11110000) == 0b11100000;
    };
    static constexpr auto starts_4_byte_sequence = [](unsigned char c) {
        return (c & 0b11111000) == 0b11110000;
    };

    string res;
    res.reserve(prefix.size() + bytes.size() + suffix.size());
    res.append(prefix);
    for (size_t i = 0; i < bytes.size(); ++i) {
        unsigned char c = bytes[i];
        // Common case: a printable character
        if ('\x20' <= c && c < '\x7f') {
            res += static_cast<char>(c);
            continue;
        }

        // Check UTF-8 correctness
        if (starts_2_byte_sequence(c) && i + 1 < bytes.size() &&
            is_a_continuation_byte(bytes[i + 1]))
        {
            res.append(bytes.data() + i, 2);
            i += 1;
            continue;
        }
        if (starts_3_byte_sequence(c) && i + 2 < bytes.size() &&
            is_a_continuation_byte(bytes[i + 1]) && is_a_continuation_byte(bytes[i + 2]))
        {
            res.append(bytes.data() + i, 3);
            i += 2;
            continue;
        }
        if (starts_4_byte_sequence(c) && i + 3 < bytes.size() &&
            is_a_continuation_byte(bytes[i + 1]) && is_a_continuation_byte(bytes[i + 2]) &&
            is_a_continuation_byte(bytes[i + 3]))
        {
            res.append(bytes.data() + i, 4);
            i += 3;
            continue;
        }

        if (c == '\n') {
            res += "\\n";
        } else if (c == '\t') {
            res += "\\t";
        } else if (c == '\r') {
            res += "\\r";
        } else if (c == '\0') {
            res += "\\0";
        } else {
            res += "\\x";
            res += dec2hex(c >> 4);
            res += dec2hex(c & 15);
        }
    }
    res.append(suffix);
    return res;
}

#pragma once

#include <simlib/string.h>

namespace jobs {

/// Append an integer @p x in binary format to the @p buff
template<class Integer>
inline typename std::enable_if<std::is_integral<Integer>::value, void>::type
	appendInt(std::string& buff, Integer x)
{
	buff.append(sizeof(x), '\0');
	for (uint i = 1, shift = 0; i <= sizeof(x); ++i, shift += 8)
		buff[buff.size() - i] = (x >> shift) & 0xFF;
}

template<class Integer>
inline typename std::enable_if<std::is_integral<Integer>::value, Integer>::type
	extractDumpedInt(StringView& dumped_str)
{
	throw_assert(dumped_str.size() >= sizeof(Integer));
	Integer x = 0;
	for (int i = sizeof(x) - 1, shift = 0; i >= 0; --i, shift += 8)
		x |= static_cast<Integer>(dumped_str[i]) << shift;
	dumped_str.removePrefix(sizeof(x));
	return x;
}

template<class Integer>
inline typename std::enable_if<std::is_integral<Integer>::value, void>::type
	extractDumpedInt(Integer& x, StringView& dumped_str)
{
	x = extractDumpedInt<Integer>(dumped_str);
}

/// Dumps @p str to binary format XXXXABC... where XXXX code string's size and
/// ABC... is the @p str and appends it to the @p buff
inline void appendDumpedString(std::string& buff, const std::string& str) {
	appendInt<uint32_t>(buff, str.size());
	buff += str;
}

/// Dumps @p str to binary format XXXXABC... where XXXX code string's size and
/// ABC... is the @p str
/*inline std::string dump(const std::string& str) {
	std::string res;
	dump(res, str);
	return res;
}*/

inline std::string extractDumpedString(StringView& dumped_str) {
	uint32_t size;
	extractDumpedInt(size, dumped_str);
	throw_assert(dumped_str.size() >= size);
	return dumped_str.extractPrefix(size).to_string();
}

// TODO: add unit tests to the above functions

struct AddProblem {
	std::string name, label;
	uint64_t memory_limit = 0; // in bytes
	uint64_t global_time_limit = 0; // in usec
	bool force_auto_limit = false;
	bool ignore_simfile = false;

	AddProblem() = default;

	AddProblem(const std::string& n, const std::string& l, uint64_t ml,
			uint64_t gtl, bool fal, bool is)
		: name {n}, label {l}, memory_limit {ml}, global_time_limit {gtl},
			force_auto_limit {fal}, ignore_simfile {is} {}

	AddProblem(StringView str) {
		name = extractDumpedString(str);
		label = extractDumpedString(str);
		extractDumpedInt(memory_limit, str);
		extractDumpedInt(global_time_limit, str);
		force_auto_limit = extractDumpedInt<uint8_t>(str);
		ignore_simfile = extractDumpedInt<uint8_t>(str);
	}

	std::string dump() {
		std::string res;
		appendDumpedString(res, name);
		appendDumpedString(res, label);
		appendInt(res, memory_limit);
		appendInt(res, global_time_limit);
		appendInt<uint8_t>(res, force_auto_limit);
		appendInt<uint8_t>(res, ignore_simfile);
		return res;
	}
};

} // namespace jobs

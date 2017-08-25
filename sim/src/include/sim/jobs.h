#pragma once

#include <sim/constants.h>
#include <simlib/mysql.h>
#include <utime.h>

namespace jobs {

/// Append an integer @p x in binary format to the @p buff
template<class Integer>
inline std::enable_if_t<std::is_integral<Integer>::value, void>
	appendDumpedInt(std::string& buff, Integer x)
{
	buff.append(sizeof(x), '\0');
	for (uint i = 1, shift = 0; i <= sizeof(x); ++i, shift += 8)
		buff[buff.size() - i] = (x >> shift) & 0xFF;
}

template<class Integer>
inline std::enable_if_t<std::is_integral<Integer>::value, Integer>
	extractDumpedInt(StringView& dumped_str)
{
	throw_assert(dumped_str.size() >= sizeof(Integer));
	Integer x = 0;
	for (int i = sizeof(x) - 1, shift = 0; i >= 0; --i, shift += 8)
		x |= ((static_cast<Integer>((uint8_t)dumped_str[i])) << shift);
	dumped_str.removePrefix(sizeof(x));
	return x;
}

template<class Integer>
inline std::enable_if_t<std::is_integral<Integer>::value, void>
	extractDumpedInt(Integer& x, StringView& dumped_str)
{
	x = extractDumpedInt<Integer>(dumped_str);
}

/// Dumps @p str to binary format XXXXABC... where XXXX code string's size and
/// ABC... is the @p str and appends it to the @p buff
inline void appendDumpedString(std::string& buff, StringView str) {
	appendDumpedInt<uint32_t>(buff, str.size());
	buff += str;
}

/// Returns dumped @p str to binary format XXXXABC... where XXXX code string's
/// size and ABC... is the @p str
inline std::string dumpString(StringView str) {
	std::string res;
	appendDumpedString(res, str);
	return res;
}

inline std::string extractDumpedString(StringView& dumped_str) {
	uint32_t size;
	extractDumpedInt(size, dumped_str);
	throw_assert(dumped_str.size() >= size);
	return dumped_str.extractPrefix(size).to_string();
}

inline std::string extractDumpedString(StringView&& dumped_str) {
	return extractDumpedString(dumped_str);
}

struct AddProblemInfo {
	std::string name, label;
	uint64_t memory_limit = 0; // in bytes
	uint64_t global_time_limit = 0; // in usec
	bool force_auto_limit = false;
	bool ignore_simfile = false;
	bool make_public = false;
	enum Stage : uint8_t { FIRST = 0, SECOND = 1 } stage = FIRST;
	int32_t previous_owner = -1;

	AddProblemInfo() = default;

	AddProblemInfo(const std::string& n, const std::string& l, uint64_t ml,
			uint64_t gtl, bool fal, bool is, bool mp, int32_t po)
		: name {n}, label {l}, memory_limit {ml}, global_time_limit {gtl},
			force_auto_limit {fal}, ignore_simfile {is}, make_public {mp},
			previous_owner{po} {}

	AddProblemInfo(StringView str) {
		name = extractDumpedString(str);
		label = extractDumpedString(str);
		extractDumpedInt(memory_limit, str);
		extractDumpedInt(global_time_limit, str);

		uint8_t mask = extractDumpedInt<uint8_t>(str);
		force_auto_limit = (mask & 1);
		ignore_simfile = (mask & 2);
		make_public = (mask & 4);

		stage = static_cast<Stage>(extractDumpedInt<uint8_t>(str));
		previous_owner = static_cast<Stage>(extractDumpedInt<int32_t>(str));
	}

	std::string dump() {
		std::string res;
		appendDumpedString(res, name);
		appendDumpedString(res, label);
		appendDumpedInt(res, memory_limit);
		appendDumpedInt(res, global_time_limit);

		uint8_t mask = force_auto_limit |
			(int(ignore_simfile) << 1) |
			(int(make_public) << 2);
		appendDumpedInt<uint8_t>(res, mask);

		appendDumpedInt<uint8_t>(res, stage);
		appendDumpedInt<int32_t>(res, previous_owner);
		return res;
	}
};

void restart_job(MySQL::Connection& mysql, StringView job_id, JobType job_type,
	StringView job_info, bool notify_job_server);

void restart_job(MySQL::Connection& mysql, StringView job_id,
	bool notify_job_server);

// Notifies the Job server that there are jobs to do
inline void notify_job_server() noexcept {
	utime(JOB_SERVER_NOTIFYING_FILE, nullptr);
}

} // namespace jobs

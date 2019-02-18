#pragma once

#include <sim/constants.h>
#include <simlib/mysql.h>
#include <utime.h>

namespace jobs {

/// Append an integer @p x in binary format to the @p buff
template<class Integer>
inline std::enable_if_t<std::is_integral<Integer>::value, void>
	appendDumped(std::string& buff, Integer x)
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
	extractDumped(Integer& x, StringView& dumped_str)
{
	x = extractDumpedInt<Integer>(dumped_str);
}

/// Dumps @p str to binary format XXXXABC... where XXXX code string's size and
/// ABC... is the @p str and appends it to the @p buff
inline void appendDumped(std::string& buff, StringView str) {
	appendDumped<uint32_t>(buff, str.size());
	buff += str;
}

template<class Rep, class Period>
inline void appendDumped(std::string& buff, const std::chrono::duration<Rep, Period>& dur) {
	appendDumped(buff, dur.count());
}

template<class Rep, class Period>
inline void extractDumped(std::chrono::duration<Rep, Period>& dur, StringView& dumped_str) {
	Rep rep;
	extractDumped(rep, dumped_str);
	dur = decltype(dur)(rep);
}

template<class T>
inline void appendDumped(std::string& buff, const Optional<T>& opt) {
	if (opt.has_value()) {
		appendDumped(buff, true);
		appendDumped(buff, opt.value());
	} else {
		appendDumped(buff, false);
	}
}

template<class T>
inline void extractDumped(Optional<T>& opt, StringView& dumped_str) {
	bool has_val;
	extractDumped(has_val, dumped_str);
	if (has_val) {
		T val;
		extractDumped(val, dumped_str);
		opt = val;
	} else {
		opt = std::nullopt;
	}
}

/// Returns dumped @p str to binary format XXXXABC... where XXXX code string's
/// size and ABC... is the @p str
inline std::string dumpString(StringView str) {
	std::string res;
	appendDumped(res, str);
	return res;
}

inline std::string extractDumpedString(StringView& dumped_str) {
	uint32_t size;
	extractDumped(size, dumped_str);
	throw_assert(dumped_str.size() >= size);
	return dumped_str.extractPrefix(size).to_string();
}

inline std::string extractDumpedString(StringView&& dumped_str) {
	return extractDumpedString(dumped_str); /* std::move() is intentionally
		omitted in order to call the above implementation */
}

struct AddProblemInfo {
	std::string name;
	std::string label;
	Optional<uint64_t> memory_limit; // in bytes
	Optional<std::chrono::nanoseconds> global_time_limit;
	bool reset_time_limits = false;
	bool ignore_simfile = false;
	bool seek_for_new_tests = false;
	bool reset_scoring = false;
	ProblemType problem_type = ProblemType::VOID;
	enum Stage : uint8_t { FIRST = 0, SECOND = 1 } stage = FIRST;

	AddProblemInfo() = default;

	AddProblemInfo(const std::string& n, const std::string& l,
			Optional<uint64_t> ml, Optional<std::chrono::nanoseconds> gtl,
			bool rtl, bool is, bool sfnt, bool rs, ProblemType pt)
		: name(n), label(l), memory_limit(ml), global_time_limit(gtl),
			reset_time_limits(rtl), ignore_simfile(is),
			seek_for_new_tests(sfnt), reset_scoring(rs), problem_type(pt) {}

	AddProblemInfo(StringView str) {
		name = extractDumpedString(str);
		label = extractDumpedString(str);
		extractDumped(memory_limit, str);
		extractDumped(global_time_limit, str);

		uint8_t mask = extractDumpedInt<uint8_t>(str);
		reset_time_limits = (mask & 1);
		ignore_simfile = (mask & 2);
		seek_for_new_tests = (mask & 4);
		reset_scoring = (mask & 8);

		problem_type = EnumVal<ProblemType>(
			extractDumpedInt<std::underlying_type_t<ProblemType>>(str));
		stage = EnumVal<Stage>(
			extractDumpedInt<std::underlying_type_t<Stage>>(str));
	}

	std::string dump() const {
		std::string res;
		appendDumped(res, name);
		appendDumped(res, label);
		appendDumped(res, memory_limit);
		appendDumped(res, global_time_limit);

		uint8_t mask = reset_time_limits | (int(ignore_simfile) << 1) |
			(int(seek_for_new_tests) << 2) | (int(reset_scoring) << 3);
		appendDumped(res, mask);

		appendDumped(res, std::underlying_type_t<ProblemType>(problem_type));
		appendDumped<std::underlying_type_t<Stage>>(res, stage);
		return res;
	}
};

struct MergeProblemsInfo {
	uint64_t target_problem_id;
	bool rejudge_transferred_submissions;

	MergeProblemsInfo() = default;

	MergeProblemsInfo(uint64_t tpid, bool rts) noexcept : target_problem_id(tpid), rejudge_transferred_submissions(rts) {}

	MergeProblemsInfo(StringView str) {
		extractDumped(target_problem_id, str);

		auto mask = extractDumpedInt<uint8_t>(str);
		rejudge_transferred_submissions = (mask & 1);
	}

	std::string dump() {
		std::string res;
		appendDumped(res, target_problem_id);

		uint8_t mask = rejudge_transferred_submissions;
		appendDumped(res, mask);
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

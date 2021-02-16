#pragma once

#include "sim/constants.hh"
#include "sim/problem.hh"
#include "simlib/mysql.hh"

#include <utility>
#include <utime.h>

namespace jobs {

/// Append an integer @p x in binary format to the @p buff
template <class Integer>
inline std::enable_if_t<std::is_integral<Integer>::value, void>
append_dumped(std::string& buff, Integer x) {
    buff.append(sizeof(x), '\0');
    for (uint i = 1, shift = 0; i <= sizeof(x); ++i, shift += 8) {
        buff[buff.size() - i] = (x >> shift) & 0xFF;
    }
}

template <class Integer>
inline std::enable_if_t<std::is_integral<Integer>::value, Integer>
extract_dumped_int(StringView& dumped_str) {
    throw_assert(dumped_str.size() >= sizeof(Integer));
    Integer x = 0;
    for (int i = sizeof(x) - 1, shift = 0; i >= 0; --i, shift += 8) {
        x |= ((static_cast<Integer>(static_cast<uint8_t>(dumped_str[i]))) << shift);
    }
    dumped_str.remove_prefix(sizeof(x));
    return x;
}

template <class Integer>
inline std::enable_if_t<std::is_integral<Integer>::value, void>
extract_dumped(Integer& x, StringView& dumped_str) {
    x = extract_dumped_int<Integer>(dumped_str);
}

/// Dumps @p str to binary format XXXXABC... where XXXX code string's size and
/// ABC... is the @p str and appends it to the @p buff
inline void append_dumped(std::string& buff, StringView str) {
    append_dumped<uint32_t>(buff, str.size());
    buff += str;
}

template <class Rep, class Period>
inline void append_dumped(std::string& buff, const std::chrono::duration<Rep, Period>& dur) {
    append_dumped(buff, dur.count());
}

template <class Rep, class Period>
inline void extract_dumped(std::chrono::duration<Rep, Period>& dur, StringView& dumped_str) {
    Rep rep;
    extract_dumped(rep, dumped_str);
    dur = decltype(dur)(rep);
}

template <class T>
inline void append_dumped(std::string& buff, const std::optional<T>& opt) {
    if (opt.has_value()) {
        append_dumped(buff, true);
        append_dumped(buff, opt.value());
    } else {
        append_dumped(buff, false);
    }
}

template <class T>
inline void extract_dumped(std::optional<T>& opt, StringView& dumped_str) {
    bool has_val = false;
    extract_dumped(has_val, dumped_str);
    if (has_val) {
        T val{};
        extract_dumped(val, dumped_str);
        opt = val;
    } else {
        opt = std::nullopt;
    }
}

/// Returns dumped @p str to binary format XXXXABC... where XXXX code string's
/// size and ABC... is the @p str
inline std::string dump_string(StringView str) {
    std::string res;
    append_dumped(res, str);
    return res;
}

inline std::string extract_dumped_string(StringView& dumped_str) {
    uint32_t size = 0;
    extract_dumped(size, dumped_str);
    throw_assert(dumped_str.size() >= size);
    return dumped_str.extract_prefix(size).to_string();
}

inline std::string extract_dumped_string(StringView&& dumped_str) {
    return extract_dumped_string(dumped_str); /* std::move() is intentionally
        omitted in order to call the above implementation */
}

struct AddProblemInfo {
    std::string name;
    std::string label;
    std::optional<uint64_t> memory_limit; // in bytes
    std::optional<std::chrono::nanoseconds> global_time_limit;
    bool reset_time_limits = false;
    bool ignore_simfile = false;
    bool seek_for_new_tests = false;
    bool reset_scoring = false;
    sim::Problem::Type problem_type = sim::Problem::Type::PRIVATE;
    enum Stage : uint8_t { FIRST = 0, SECOND = 1 } stage = FIRST; // TODO: remove this

    AddProblemInfo() = default;

    AddProblemInfo(
        std::string n, std::string l, std::optional<uint64_t> ml,
        std::optional<std::chrono::nanoseconds> gtl, bool rtl, bool is, bool sfnt, bool rs,
        sim::Problem::Type pt)
    : name(std::move(n))
    , label(std::move(l))
    , memory_limit(ml)
    , global_time_limit(gtl)
    , reset_time_limits(rtl)
    , ignore_simfile(is)
    , seek_for_new_tests(sfnt)
    , reset_scoring(rs)
    , problem_type(pt) {}

    // NOLINTNEXTLINE(google-explicit-constructor)
    AddProblemInfo(StringView str) {
        name = extract_dumped_string(str);
        label = extract_dumped_string(str);
        extract_dumped(memory_limit, str);
        extract_dumped(global_time_limit, str);

        uint8_t mask = extract_dumped_int<uint8_t>(str);
        reset_time_limits = (mask & 1);
        ignore_simfile = (mask & 2);
        seek_for_new_tests = (mask & 4);
        reset_scoring = (mask & 8);

        problem_type = EnumVal<sim::Problem::Type>(
            extract_dumped_int<std::underlying_type_t<sim::Problem::Type>>(str));
        stage = EnumVal<Stage>(extract_dumped_int<std::underlying_type_t<Stage>>(str));
    }

    [[nodiscard]] std::string dump() const {
        std::string res;
        append_dumped(res, name);
        append_dumped(res, label);
        append_dumped(res, memory_limit);
        append_dumped(res, global_time_limit);

        uint8_t mask = reset_time_limits | (int(ignore_simfile) << 1) |
            (int(seek_for_new_tests) << 2) | (int(reset_scoring) << 3);
        append_dumped(res, mask);

        append_dumped(res, EnumVal(problem_type).int_val());
        append_dumped<std::underlying_type_t<Stage>>(res, stage);
        return res;
    }
};

struct MergeProblemsInfo {
    uint64_t target_problem_id{};
    bool rejudge_transferred_submissions{};

    MergeProblemsInfo() = default;

    MergeProblemsInfo(uint64_t tpid, bool rts) noexcept
    : target_problem_id(tpid)
    , rejudge_transferred_submissions(rts) {}

    explicit MergeProblemsInfo(StringView str) {
        extract_dumped(target_problem_id, str);

        auto mask = extract_dumped_int<uint8_t>(str);
        rejudge_transferred_submissions = (mask & 1);
    }

    [[nodiscard]] std::string dump() const {
        std::string res;
        append_dumped(res, target_problem_id);

        uint8_t mask = rejudge_transferred_submissions;
        append_dumped(res, mask);
        return res;
    }
};

struct ChangeProblemStatementInfo {
    std::string new_statement_path;

    ChangeProblemStatementInfo() = default;

    explicit ChangeProblemStatementInfo(StringView nsp)
    : new_statement_path(nsp.to_string()) {}

    [[nodiscard]] std::string dump() const { return new_statement_path; }
};

struct MergeUsersInfo {
    uint64_t target_user_id{};

    MergeUsersInfo() = default;

    explicit MergeUsersInfo(uint64_t target_uid) noexcept
    : target_user_id(target_uid) {}

    explicit MergeUsersInfo(StringView str) { extract_dumped(target_user_id, str); }

    [[nodiscard]] std::string dump() const {
        std::string res;
        append_dumped(res, target_user_id);
        return res;
    }
};

void restart_job(
    MySQL::Connection& mysql, StringView job_id, JobType job_type, StringView job_info,
    bool notify_job_server);

void restart_job(MySQL::Connection& mysql, StringView job_id, bool notify_job_server);

// Notifies the Job server that there are jobs to do
inline void notify_job_server() noexcept { utime(JOB_SERVER_NOTIFYING_FILE, nullptr); }

} // namespace jobs

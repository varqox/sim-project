#pragma once

#include <sim/jobs/old_job.hh>
#include <sim/old_mysql/old_mysql.hh>
#include <sim/problems/old_problem.hh>
#include <sim/users/old_user.hh>
#include <utility>

namespace sim::jobs {

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

struct MergeProblemsInfo {
    decltype(sim::problems::OldProblem::id) target_problem_id{};
    static_assert(
        sizeof(target_problem_id) == 8, "Changing size needs updating column info in jobs table"
    );
    bool rejudge_transferred_submissions{};

    MergeProblemsInfo() = default;

    MergeProblemsInfo(decltype(sim::problems::OldProblem::id) tpid, bool rts) noexcept
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

    explicit ChangeProblemStatementInfo(StringView nsp) : new_statement_path(nsp.to_string()) {}

    [[nodiscard]] std::string dump() const { return new_statement_path; }
};

struct MergeUsersInfo {
    decltype(sim::users::User::id) target_user_id{};
    static_assert(
        sizeof(target_user_id) == 8, "Changing size needs updating column info in jobs table"
    );

    MergeUsersInfo() = default;

    explicit MergeUsersInfo(decltype(sim::users::User::id) target_uid) noexcept
    : target_user_id(target_uid) {}

    explicit MergeUsersInfo(StringView str) { extract_dumped(target_user_id, str); }

    [[nodiscard]] std::string dump() const {
        std::string res;
        append_dumped(res, target_user_id);
        return res;
    }
};

void restart_job(mysql::Connection& mysql, StringView job_id, bool notify_job_server);

// Notifies the Job server that there are jobs to do
void notify_job_server() noexcept;

} // namespace sim::jobs

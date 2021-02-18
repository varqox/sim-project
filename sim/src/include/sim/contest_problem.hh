#pragma once

#include "sim/api/enum.hh"
#include "sim/api/int.hh"
#include "sim/api/string.hh"
#include "sim/blob_field.hh"
#include "sim/constants.hh" // TODO: remove when ExtraIterateData does not use it
#include "sim/contest_permissions.hh"
#include "sim/mysql.hh"
#include "sim/problem.hh"
#include "sim/problem_permissions.hh"
#include "sim/varchar_field.hh"
#include "simlib/concat_tostr.hh"
#include "simlib/string_view.hh"

namespace sim {

struct ContestProblem {
    enum class MethodOfChoosingFinalSubmission : uint8_t {
        LATEST_COMPILING = 0,
        HIGHEST_SCORE = 1
    };

    enum class ScoreRevealing : uint8_t {
        NONE = 0,
        ONLY_SCORE = 1,
        SCORE_AND_FULL_STATUS = 2,
    };

    uintmax_t id;
    uintmax_t contest_round_id;
    uintmax_t contest_id;

    constexpr static const char problem_id_var_name[] = "problem_id";
    constexpr static const char problem_id_var_descr[] = "Problem ID";
    api::Int<uintmax_t, problem_id_var_name, problem_id_var_descr, 1> problem_id;

    constexpr static const char name_var_name[] = "name";
    constexpr static const char name_var_descr[] = "Problem's name";
    api::String<VarcharField<128>, name_var_name, name_var_descr> name;

    uintmax_t item;
    api::Enum<MethodOfChoosingFinalSubmission> method_of_choosing_final_submission;
    api::Enum<ScoreRevealing> score_revealing;
};

namespace api {

template <>
constexpr CStringView Enum<ContestProblem::ScoreRevealing>::api_var_name = "score_revealing";
template <>
constexpr CStringView Enum<ContestProblem::ScoreRevealing>::api_var_description =
    "Score revealing";

template <>
constexpr std::optional<Enum<ContestProblem::ScoreRevealing>>
Enum<ContestProblem::ScoreRevealing>::from_str_impl(StringView str) {
    using SR = ContestProblem::ScoreRevealing;
    if (str == "none") {
        return SR::NONE;
    }
    if (str == "only_score") {
        return SR::ONLY_SCORE;
    }
    if (str == "score_and_full_status") {
        return SR::SCORE_AND_FULL_STATUS;
    }
    return std::nullopt;
}

template <>
constexpr CStringView Enum<ContestProblem::ScoreRevealing>::to_str() const noexcept {
    using SR = ContestProblem::ScoreRevealing;
    switch (*this) {
    case SR::NONE: return "none";
    case SR::ONLY_SCORE: return "only_score";
    case SR::SCORE_AND_FULL_STATUS: return "score_and_full_status";
    }
    return "unknown";
}

template <>
constexpr CStringView Enum<ContestProblem::MethodOfChoosingFinalSubmission>::api_var_name =
    "method_of_choosing_final_submission";
template <>
constexpr CStringView
    Enum<ContestProblem::MethodOfChoosingFinalSubmission>::api_var_description =
        "Method of choosing final submission";

template <>
constexpr std::optional<Enum<ContestProblem::MethodOfChoosingFinalSubmission>>
Enum<ContestProblem::MethodOfChoosingFinalSubmission>::from_str_impl(StringView val) {
    using MOCFS = ContestProblem::MethodOfChoosingFinalSubmission;
    if (val == "latest_compiling") {
        return MOCFS::LATEST_COMPILING;
    }
    if (val == "highest_score") {
        return MOCFS::HIGHEST_SCORE;
    }
    return std::nullopt;
}

template <>
constexpr CStringView
Enum<ContestProblem::MethodOfChoosingFinalSubmission>::to_str() const noexcept {
    using MOCFS = ContestProblem::MethodOfChoosingFinalSubmission;
    switch (*this) {
    case MOCFS::LATEST_COMPILING: return "latest_compiling";
    case MOCFS::HIGHEST_SCORE: return "highest_score";
    }
    return "unknown";
}

} // namespace api

namespace contest_problem {

enum class IterateIdKind { CONTEST, CONTEST_ROUND, CONTEST_PROBLEM };

struct ExtraIterateData {
    decltype(Problem::label) problem_label;
    std::optional<EnumVal<SubmissionStatus>> initial_final_submission_initial_status;
    std::optional<EnumVal<SubmissionStatus>> final_submission_full_status;
    problem::Permissions problem_perms;
};

template <class T, class U, class Func>
void iterate(
    MySQL::Connection& mysql, IterateIdKind id_kind, T&& id,
    contest::Permissions contest_perms, std::optional<U> user_id,
    std::optional<User::Type> user_type, CStringView curr_date,
    Func&& contest_problem_processor) {
    STACK_UNWINDING_MARK;
    static_assert(std::is_invocable_v<Func, const ContestProblem&, const ExtraIterateData&>);

    throw_assert(user_id.has_value() == user_type.has_value());

    StringView id_field = [&]() -> StringView {
        switch (id_kind) {
        case IterateIdKind::CONTEST: return "cp.contest_id";
        case IterateIdKind::CONTEST_ROUND: return "cp.contest_round_id";
        case IterateIdKind::CONTEST_PROBLEM: return "cp.id";
        }
        __builtin_unreachable();
    }();

    bool show_all_rounds = uint(contest_perms & contest::Permissions::ADMIN);

    auto stmt = mysql.prepare(
        "SELECT cp.id, cp.contest_round_id, cp.contest_id, cp.problem_id,"
        " cp.name, cp.item, cp.method_of_choosing_final_submission, cp.score_revealing,"
        " p.label, si.initial_status, sf.full_status, p.owner, p.type "
        "FROM contest_problems cp ",
        (show_all_rounds ? ""
                         : "JOIN contest_rounds cr ON cr.id=cp.contest_round_id "
                           "AND cr.begins<=? "),
        "JOIN problems p ON p.id=cp.problem_id "
        "LEFT JOIN submissions si ON si.owner=? AND si.contest_problem_id=cp.id"
        " AND si.contest_initial_final=1 "
        "LEFT JOIN submissions sf ON sf.owner=? AND sf.contest_problem_id=cp.id"
        " AND sf.contest_final=1 "
        "WHERE ",
        id_field, "=?");

    if (show_all_rounds) {
        stmt.bind_and_execute(user_id, user_id, id);
    } else {
        stmt.bind_and_execute(curr_date, user_id, user_id, id);
    }

    ContestProblem cp;
    ExtraIterateData extra_data;
    MySQL::Optional<EnumVal<SubmissionStatus>> m_initial_final_status;
    MySQL::Optional<EnumVal<SubmissionStatus>> m_final_status;
    MySQL::Optional<decltype(Problem::owner)::value_type> m_problem_owner;
    decltype(Problem::type) m_problem_type;
    stmt.res_bind_all(
        cp.id, cp.contest_round_id, cp.contest_id, cp.problem_id, cp.name, cp.item,
        cp.method_of_choosing_final_submission, cp.score_revealing, extra_data.problem_label,
        m_initial_final_status, m_final_status, m_problem_owner, m_problem_type);

    while (stmt.next()) {
        extra_data.initial_final_submission_initial_status = m_initial_final_status;
        extra_data.final_submission_full_status = m_final_status;
        extra_data.problem_perms =
            problem::get_permissions(user_id, user_type, m_problem_owner, m_problem_type);
        contest_problem_processor(
            static_cast<const ContestProblem&>(cp),
            static_cast<const ExtraIterateData&>(extra_data));
    }
}

} // namespace contest_problem
} // namespace sim

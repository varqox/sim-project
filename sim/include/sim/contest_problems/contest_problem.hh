#pragma once

#include "sim/problems/problem.hh"
#include "sim/sql_fields/varchar.hh"
#include "sim/web_api/enum.hh"
#include "sim/web_api/int.hh"
#include "sim/web_api/string.hh"
#include "simlib/string_view.hh"

#include <cstdint>

namespace sim::contest_problems {

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

    uint64_t id;
    uint64_t contest_round_id;
    uint64_t contest_id;

    constexpr static const char problem_id_var_name[] = "problem_id";
    constexpr static const char problem_id_var_descr[] = "Problem ID";
    web_api::Int<decltype(problems::Problem::id), problem_id_var_name, problem_id_var_descr, 1>
        problem_id;

    constexpr static const char name_var_name[] = "name";
    constexpr static const char name_var_descr[] = "Problem's name";
    web_api::String<sql_fields::Varchar<128>, name_var_name, name_var_descr> name;

    uint64_t item;
    web_api::Enum<MethodOfChoosingFinalSubmission> method_of_choosing_final_submission;
    web_api::Enum<ScoreRevealing> score_revealing;
};

} // namespace sim::contest_problems

namespace sim::web_api {

template <>
constexpr CStringView Enum<contest_problems::ContestProblem::ScoreRevealing>::api_var_name =
    "score_revealing";
template <>
constexpr CStringView
    Enum<contest_problems::ContestProblem::ScoreRevealing>::api_var_description =
        "Score revealing";

template <>
constexpr std::optional<Enum<contest_problems::ContestProblem::ScoreRevealing>>
Enum<contest_problems::ContestProblem::ScoreRevealing>::from_str_impl(StringView str) {
    using SR = contest_problems::ContestProblem::ScoreRevealing;
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
constexpr CStringView
Enum<contest_problems::ContestProblem::ScoreRevealing>::to_str() const noexcept {
    using SR = contest_problems::ContestProblem::ScoreRevealing;
    switch (*this) {
    case SR::NONE: return "none";
    case SR::ONLY_SCORE: return "only_score";
    case SR::SCORE_AND_FULL_STATUS: return "score_and_full_status";
    }
    return "unknown";
}

template <>
constexpr CStringView
    Enum<contest_problems::ContestProblem::MethodOfChoosingFinalSubmission>::api_var_name =
        "method_of_choosing_final_submission";
template <>
constexpr CStringView Enum<
    contest_problems::ContestProblem::MethodOfChoosingFinalSubmission>::api_var_description =
    "Method of choosing final submission";

template <>
constexpr std::optional<
    Enum<contest_problems::ContestProblem::MethodOfChoosingFinalSubmission>>
Enum<contest_problems::ContestProblem::MethodOfChoosingFinalSubmission>::from_str_impl(
    StringView val) {
    using MOCFS = contest_problems::ContestProblem::MethodOfChoosingFinalSubmission;
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
Enum<contest_problems::ContestProblem::MethodOfChoosingFinalSubmission>::to_str()
    const noexcept {
    using MOCFS = contest_problems::ContestProblem::MethodOfChoosingFinalSubmission;
    switch (*this) {
    case MOCFS::LATEST_COMPILING: return "latest_compiling";
    case MOCFS::HIGHEST_SCORE: return "highest_score";
    }
    return "unknown";
}

} // namespace sim::web_api

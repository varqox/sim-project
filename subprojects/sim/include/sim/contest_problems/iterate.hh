#pragma once

#include <sim/contest_problems/old_contest_problem.hh>
#include <sim/contests/permissions.hh>
#include <sim/problems/old_problem.hh>
#include <sim/problems/permissions.hh>
#include <sim/submissions/old_submission.hh>

namespace sim::contest_problems {

enum class IterateIdKind { CONTEST, CONTEST_ROUND, CONTEST_PROBLEM };

struct ExtraIterateData {
    decltype(problems::OldProblem::label) problem_label;
    std::optional<EnumVal<sim::submissions::OldSubmission::Status>>
        initial_final_submission_initial_status;
    std::optional<EnumVal<sim::submissions::OldSubmission::Status>> final_submission_full_status;
    problems::Permissions problem_perms;
};

template <class T, class U, class Func>
void iterate(
    sim::mysql::Connection& mysql,
    IterateIdKind id_kind,
    T&& id,
    contests::Permissions contest_perms,
    std::optional<U> user_id,
    std::optional<users::User::Type> user_type,
    CStringView curr_date,
    Func&& contest_problem_processor
) {
    STACK_UNWINDING_MARK;
    static_assert(std::is_invocable_v<Func, const OldContestProblem&, const ExtraIterateData&>);

    throw_assert(user_id.has_value() == user_type.has_value());

    StringView id_field = [&]() -> StringView {
        switch (id_kind) {
        case IterateIdKind::CONTEST: return "cp.contest_id";
        case IterateIdKind::CONTEST_ROUND: return "cp.contest_round_id";
        case IterateIdKind::CONTEST_PROBLEM: return "cp.id";
        }
        __builtin_unreachable();
    }();

    bool show_all_rounds = uint(contest_perms & contests::Permissions::ADMIN);

    auto old_mysql = old_mysql::ConnectionView{mysql};
    auto stmt = old_mysql.prepare(
        "SELECT cp.id, cp.contest_round_id, cp.contest_id, cp.problem_id,"
        " cp.name, cp.item, cp.method_of_choosing_final_submission, cp.score_revealing,"
        " p.label, si.initial_status, sf.full_status, p.owner_id, p.type "
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
        id_field,
        "=?"
    );

    if (show_all_rounds) {
        stmt.bind_and_execute(user_id, user_id, id);
    } else {
        stmt.bind_and_execute(curr_date, user_id, user_id, id);
    }

    OldContestProblem cp;
    ExtraIterateData extra_data;
    old_mysql::Optional<EnumVal<sim::submissions::OldSubmission::Status>> m_initial_final_status;
    old_mysql::Optional<EnumVal<sim::submissions::OldSubmission::Status>> m_final_status;
    old_mysql::Optional<decltype(problems::OldProblem::owner_id)::value_type> m_problem_owner_id;
    decltype(problems::OldProblem::type) m_problem_type;
    stmt.res_bind_all(
        cp.id,
        cp.contest_round_id,
        cp.contest_id,
        cp.problem_id,
        cp.name,
        cp.item,
        cp.method_of_choosing_final_submission,
        cp.score_revealing,
        extra_data.problem_label,
        m_initial_final_status,
        m_final_status,
        m_problem_owner_id,
        m_problem_type
    );

    while (stmt.next()) {
        extra_data.initial_final_submission_initial_status = m_initial_final_status;
        extra_data.final_submission_full_status = m_final_status;
        extra_data.problem_perms =
            problems::get_permissions(user_id, user_type, m_problem_owner_id, m_problem_type);
        contest_problem_processor(
            static_cast<const OldContestProblem&>(cp),
            static_cast<const ExtraIterateData&>(extra_data)
        );
    }
}

} // namespace sim::contest_problems

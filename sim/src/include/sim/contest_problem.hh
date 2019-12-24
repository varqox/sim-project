#pragma once

#include "contest_permissions.hh"
#include "mysql.h"
#include "problem.hh"
#include "problem_permissions.hh"
#include "varchar_field.hh"

#include "constants.h" // TODO: remove when ExtraIterateData does not use it

namespace sim {

struct ContestProblem {
	enum class FinalSubmissionSelectingMethod : uint8_t {
		LAST_COMPILING = 0,
		WITH_HIGHEST_SCORE = 1
	};

	enum class ScoreRevealingMode : uint8_t {
		NONE = 0,
		ONLY_SCORE = 1,
		SCORE_AND_FULL_STATUS = 2,
	};

	uintmax_t id;
	uintmax_t contest_round_id;
	uintmax_t contest_id;
	uintmax_t problem_id;
	VarcharField<128> name;
	uintmax_t item;
	EnumVal<FinalSubmissionSelectingMethod> final_selecting_method;
	EnumVal<ScoreRevealingMode> score_revealing;
};

namespace contest_problem {

enum class IterateIdKind { CONTEST, CONTEST_ROUND, CONTEST_PROBLEM };

struct ExtraIterateData {
	decltype(Problem::label) problem_label;
	std::optional<EnumVal<SubmissionStatus>>
	   initial_final_submission_initial_status;
	std::optional<EnumVal<SubmissionStatus>> final_submission_full_status;
	problem::Permissions problem_perms;
};

template <class T, class U, class Func>
void iterate(MySQL::Connection& mysql, IterateIdKind id_kind, T&& id,
             contest::Permissions contest_perms, std::optional<U> user_id,
             std::optional<User::Type> user_type, CStringView curr_date,
             Func&& contest_problem_processor) {
	STACK_UNWINDING_MARK;
	static_assert(std::is_invocable_v<Func, const ContestProblem&,
	                                  const ExtraIterateData&>);

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
	   " cp.name, cp.item, cp.final_selecting_method, cp.score_revealing,"
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

	if (show_all_rounds)
		stmt.bindAndExecute(user_id, user_id, id);
	else
		stmt.bindAndExecute(curr_date, user_id, user_id, id);

	ContestProblem cp;
	ExtraIterateData extra_data;
	MySQL::Optional<EnumVal<SubmissionStatus>> m_initial_final_status;
	MySQL::Optional<EnumVal<SubmissionStatus>> m_final_status;
	MySQL::Optional<decltype(Problem::owner)::value_type> m_problem_owner;
	decltype(Problem::type) m_problem_type;
	stmt.res_bind_all(cp.id, cp.contest_round_id, cp.contest_id, cp.problem_id,
	                  cp.name, cp.item, cp.final_selecting_method,
	                  cp.score_revealing, extra_data.problem_label,
	                  m_initial_final_status, m_final_status, m_problem_owner,
	                  m_problem_type);

	while (stmt.next()) {
		extra_data.initial_final_submission_initial_status =
		   m_initial_final_status;
		extra_data.final_submission_full_status = m_final_status;
		extra_data.problem_perms = problem::get_permissions(
		   user_id, user_type, m_problem_owner, m_problem_type);
		contest_problem_processor(
		   static_cast<const ContestProblem&>(cp),
		   static_cast<const ExtraIterateData&>(extra_data));
	}
}

} // namespace contest_problem
} // namespace sim

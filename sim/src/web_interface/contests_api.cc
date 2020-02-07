#include "sim.h"

#include <cstdint>
#include <map>
#include <sim/contest.hh>
#include <sim/contest_permissions.hh>
#include <sim/contest_problem.hh>
#include <sim/contest_round.hh>
#include <sim/contest_user.hh>
#include <sim/inf_datetime.hh>
#include <sim/jobs.h>
#include <sim/utilities.h>

using sim::Contest;
using sim::ContestProblem;
using sim::ContestRound;
using sim::ContestUser;
using sim::inf_timestamp_to_InfDatetime;
using sim::InfDatetime;
using sim::is_safe_inf_timestamp;
using sim::Problem;
using sim::User;
using std::map;
using std::optional;
using std::pair;

inline bool whether_to_show_full_status(
   sim::contest::Permissions cperms, InfDatetime full_results,
   const decltype(mysql_date())& curr_mysql_date,
   ContestProblem::ScoreRevealingMode score_revealing) {
	// TODO: check append_submission_status() for making it use the new function
	if (uint(cperms & sim::contest::Permissions::ADMIN))
		return true;

	if (full_results <= curr_mysql_date)
		return true;

	switch (score_revealing) {
	case ContestProblem::ScoreRevealingMode::NONE: return false;
	case ContestProblem::ScoreRevealingMode::ONLY_SCORE: return false;
	case ContestProblem::ScoreRevealingMode::SCORE_AND_FULL_STATUS: return true;
	}

	__builtin_unreachable();
}

inline bool
whether_to_show_score(sim::contest::Permissions cperms,
                      InfDatetime full_results,
                      const decltype(mysql_date())& curr_mysql_date,
                      ContestProblem::ScoreRevealingMode score_revealing) {
	// TODO: check append_submission_status() for making it use the new function
	if (uint(cperms & sim::contest::Permissions::ADMIN))
		return true;

	if (full_results <= curr_mysql_date)
		return true;

	switch (score_revealing) {
	case ContestProblem::ScoreRevealingMode::NONE: return false;
	case ContestProblem::ScoreRevealingMode::ONLY_SCORE: return true;
	case ContestProblem::ScoreRevealingMode::SCORE_AND_FULL_STATUS: return true;
	}

	__builtin_unreachable();
}

inline static InplaceBuff<32>
color_class_json(sim::contest::Permissions cperms, InfDatetime full_results,
                 const decltype(mysql_date())& curr_mysql_date,
                 optional<SubmissionStatus> full_status,
                 optional<SubmissionStatus> initial_status,
                 ContestProblem::ScoreRevealingMode score_revealing) {

	if (whether_to_show_full_status(cperms, full_results, curr_mysql_date,
	                                score_revealing)) {
		if (full_status.has_value())
			return concat<32>("\"", css_color_class(full_status.value()), "\"");
	} else if (initial_status.has_value()) {
		return concat<32>("\"initial ", css_color_class(initial_status.value()),
		                  "\"");
	}

	return concat<32>("null");
}

template <class T>
static void
append_contest_actions_str(T& str,
                           sim::contest::OverallPermissions overall_perms,
                           sim::contest::Permissions perms) {
	STACK_UNWINDING_MARK;
	using OP = sim::contest::OverallPermissions;
	using P = sim::contest::Permissions;

	str.append('"');
	if (uint(overall_perms & (OP::ADD_PUBLIC | OP::ADD_PRIVATE)))
		str.append('a');
	if (uint(perms & P::MAKE_PUBLIC))
		str.append('M');
	if (uint(perms & P::VIEW))
		str.append('v');
	if (uint(perms & P::PARTICIPATE))
		str.append('p');
	if (uint(perms & P::ADMIN))
		str.append('A');
	if (uint(perms & P::DELETE))
		str.append('D');
	str.append('"');
}

static constexpr const char*
to_json(ContestProblem::FinalSubmissionSelectingMethod fssm) {
	switch (fssm) {
	case ContestProblem::FinalSubmissionSelectingMethod::LAST_COMPILING:
		return "\"LC\"";
	case ContestProblem::FinalSubmissionSelectingMethod::WITH_HIGHEST_SCORE:
		return "\"WHS\"";
	}

	return "\"unknown\"";
}

static constexpr const char* to_json(ContestProblem::ScoreRevealingMode srm) {
	switch (srm) {
	case ContestProblem::ScoreRevealingMode::NONE: return "\"none\"";
	case ContestProblem::ScoreRevealingMode::ONLY_SCORE:
		return "\"only_score\"";
	case ContestProblem::ScoreRevealingMode::SCORE_AND_FULL_STATUS:
		return "\"score_and_full_status\"";
	}

	return "\"unknown\"";
}

namespace {

class ContestInfoResponseBuilder {
	using ContentType = decltype(server::HttpResponse::content)&;
	ContentType& content_;
	sim::contest::OverallPermissions contest_overall_perms_;
	sim::contest::Permissions contest_perms_;
	std::string curr_date_;
	AVLDictMap<uint64_t, InfDatetime> round_to_full_results_;

public:
	ContestInfoResponseBuilder(
	   ContentType& resp_content,
	   sim::contest::OverallPermissions contest_overall_perms,
	   sim::contest::Permissions contest_perms, std::string curr_date)
	   : content_(resp_content), contest_overall_perms_(contest_overall_perms),
	     contest_perms_(contest_perms), curr_date_(std::move(curr_date)) {}

	void append_field_names() {
		// clang-format off
		content_.append(
		   "[\n{\"fields\":["
		          "{\"name\":\"contest\",\"fields\":["
		              "\"id\","
		              "\"name\","
		              "\"is_public\","
		              "\"actions\""
		          "]},"
		          "{\"name\":\"rounds\",\"columns\":["
		              "\"id\","
		              "\"name\","
		              "\"item\","
		              "\"ranking_exposure\","
		              "\"begins\","
		              "\"full_results\","
		              "\"ends\""
		          "]},"
		          "{\"name\":\"problems\",\"columns\":["
		              "\"id\","
		              "\"round_id\","
		              "\"problem_id\","
		              "\"can_view_problem\","
		              "\"problem_label\","
		              "\"name\","
		              "\"item\","
		              "\"final_selecting_method\","
		              "\"score_revealing\","
		              "\"color_class\""
		          "]}"
		      "]},");
		// clang-format on
	}

	void append_contest(const Contest& contest) {
		content_.append("\n[", contest.id, ',', jsonStringify(contest.name),
		                (contest.is_public ? ",true," : ",false,"));
		append_contest_actions_str(content_, contest_overall_perms_,
		                           contest_perms_);

		content_.append("],\n["); // TODO: this is ugly...
	}

	void append_round(const ContestRound& contest_round) {
		round_to_full_results_.emplace(contest_round.id,
		                               contest_round.full_results);
		// clang-format off
		content_.append(
		   "\n[", contest_round.id, ',',
		   jsonStringify(contest_round.name), ',',
		   contest_round.item, ","
		   "\"", contest_round.ranking_exposure.as_inf_datetime().to_api_str(), "\","
		   "\"", contest_round.begins.as_inf_datetime().to_api_str(), "\","
		   "\"", contest_round.full_results.as_inf_datetime().to_api_str(), "\","
		   "\"", contest_round.ends.as_inf_datetime().to_api_str(), "\"],");
		// clang-format on
	}

	void start_appending_problems() {
		if (content_.back() == ',')
			--content_.size;
		content_.append("\n],\n[");
	}

	void
	append_problem(const ContestProblem& contest_problem,
	               const sim::contest_problem::ExtraIterateData extra_data) {
		auto& cp = contest_problem;
		// clang-format off
		content_.append(
		   "\n[", cp.id, ',',
		   cp.contest_round_id, ',',
		   cp.problem_id, ',',
		   (uint(extra_data.problem_perms & sim::problem::Permissions::VIEW) ? "true," : "false,"),
		   jsonStringify(extra_data.problem_label), ',',
		   jsonStringify(cp.name), ',',
		   cp.item, ',',
		   to_json(cp.final_selecting_method), ',',
		   to_json(cp.score_revealing), ',',
		   color_class_json(
		      contest_perms_, round_to_full_results_[cp.contest_round_id],
		      curr_date_, extra_data.final_submission_full_status,
		      extra_data.initial_final_submission_initial_status,
		      cp.score_revealing),
		   "],");
		// clang-format on
	}

	void stop_appending_problems() {
		if (content_.back() == ',')
			--content_.size;
		content_.append("\n]\n]");
	}
};

} // namespace

void Sim::api_contests() {
	STACK_UNWINDING_MARK;

	// We may read data several times (permission checking), so transaction is
	// used to ensure data consistency
	auto transaction = mysql.start_transaction();

	InplaceBuff<512> qfields, qwhere;
	qfields.append("SELECT c.id, c.name, c.is_public, cu.mode");
	qwhere.append(" FROM contests c LEFT JOIN contest_users cu ON "
	              "cu.contest_id=c.id AND cu.user_id=",
	              (session_is_open ? session_user_id : StringView("''")));

	enum ColumnIdx { CID, CNAME, IS_PUBLIC, USER_MODE };

	auto qwhere_append = [&, where_was_added = false](auto&&... args) mutable {
		if (where_was_added) {
			qwhere.append(" AND ", std::forward<decltype(args)>(args)...);
		} else {
			qwhere.append(" WHERE ", std::forward<decltype(args)>(args)...);
			where_was_added = true;
		}
	};

	// Get the overall permissions to the contests list
	using CO_PERMS = sim::contest::OverallPermissions;
	CO_PERMS contest_overall_perms = sim::contest::get_overall_permissions(
	   (session_is_open ? optional(session_user_type) : std::nullopt));
	// Choose contests to select
	if (uint(~contest_overall_perms & CO_PERMS::VIEW_ALL)) {
		if (uint(contest_overall_perms & CO_PERMS::VIEW_PUBLIC)) {
			if (session_is_open)
				qwhere_append("(c.is_public=1 OR cu.mode IS NOT NULL)");
			else
				qwhere_append("c.is_public=1");
		} else {
			if (session_is_open)
				qwhere_append("cu.mode IS NOT NULL");
			else
				qwhere_append("0=1");
		}
	}

	// Process restrictions
	auto rows_limit = API_FIRST_QUERY_ROWS_LIMIT;
	StringView next_arg = url_args.extractNextArg();
	for (uint mask = 0; next_arg.size(); next_arg = url_args.extractNextArg()) {
		constexpr uint ID_COND = 1;
		constexpr uint PUBLIC_COND = 2;

		auto arg = decodeURI(next_arg);
		char cond = arg[0];
		StringView arg_id = StringView(arg).substr(1);

		if (cond == 'p' and ~mask & PUBLIC_COND) { // Is public
			if (arg_id == "Y") {
				qwhere_append("c.is_public=1");
			} else if (arg_id == "N") {
				qwhere_append("c.is_public=0");
			} else {
				return api_error400(intentionalUnsafeStringView(
				   concat("Invalid is_public condition: ", arg_id)));
			}

			mask |= PUBLIC_COND;

		} else if (not isDigit(arg_id)) {
			return api_error400();

		} else if (is_one_of(cond, '=', '<', '>') and
		           ~mask & ID_COND) { // conditional
			rows_limit = API_OTHER_QUERY_ROWS_LIMIT;
			qwhere_append("c.id", arg);
			mask |= ID_COND;

		} else {
			return api_error400();
		}
	}

	// Execute query
	qfields.append(qwhere, " ORDER BY c.id DESC LIMIT ", rows_limit);
	auto res = mysql.query(qfields);

	// Column names
	// clang-format off
	append("[\n{\"columns\":["
	           "\"id\","
	           "\"name\","
	           "\"is_public\","
	           "\"actions\""
	       "]}");
	// clang-format on

	while (res.next()) {
		StringView cid = res[CID];
		StringView name = res[CNAME];
		bool is_public = strtoull(res[IS_PUBLIC]);
		auto cumode =
		   (res.is_null(USER_MODE)
		       ? std::nullopt
		       : optional(ContestUser::Mode(strtoull(res[USER_MODE]))));
		auto contest_perms = sim::contest::get_permissions(
		   (session_is_open ? optional(session_user_type) : std::nullopt),
		   is_public, cumode);

		// Id
		append(",\n[", cid, ",");
		// Name
		append(jsonStringify(name), ',');
		// Is public
		append(is_public ? "true," : "false,");

		// Append what buttons to show
		append_contest_actions_str(resp.content, contest_overall_perms,
		                           contest_perms);
		append(']');
	}

	append("\n]");
}

void Sim::api_contest() {
	STACK_UNWINDING_MARK;

	auto contest_overall_perms = sim::contest::get_overall_permissions(
	   (session_is_open ? optional(session_user_type) : std::nullopt));

	StringView next_arg = url_args.extractNextArg();
	if (next_arg == "create") {
		return api_contest_create(contest_overall_perms);
	} else if (next_arg == "clone") {
		return api_contest_clone(contest_overall_perms);
	} else if (next_arg.empty()) {
		return api_error400();
	} else if (next_arg[0] == 'r' and isDigit(next_arg.substr(1))) {
		// Select by contest round id
		return api_contest_round(next_arg.substr(1));
	} else if (next_arg[0] == 'p' and isDigit(next_arg.substr(1))) {
		// Select by contest problem id
		return api_contest_problem(next_arg.substr(1));
	} else if (not(next_arg[0] == 'c' and isDigit(next_arg.substr(1)))) {
		return api_error404();
	}

	StringView contest_id = next_arg.substr(1);

	// We read data in several queries - transaction will make the data
	// consistent
	auto transaction = mysql.start_transaction();
	auto curr_date = mysql_date();

	auto contest_opt = sim::contest::get(
	   mysql, sim::contest::GetIdKind::CONTEST, contest_id,
	   (session_is_open ? optional {session_user_id} : std::nullopt),
	   curr_date);
	if (not contest_opt)
		return api_error404();

	auto& [contest, contest_perms] = contest_opt.value();
	if (uint(~contest_perms & sim::contest::Permissions::VIEW))
		return api_error403(); // Could not participate

	next_arg = url_args.extractNextArg();
	if (next_arg == "ranking") {
		transaction.rollback(); // We only read data...
		return api_contest_ranking(contest_perms, "contest_id", contest_id);
	} else if (next_arg == "edit") {
		transaction.rollback(); // We only read data...
		return api_contest_edit(contest_id, contest_perms, contest.is_public);
	} else if (next_arg == "delete") {
		transaction.rollback(); // We only read data...
		return api_contest_delete(contest_id, contest_perms);
	} else if (next_arg == "create_round") {
		transaction.rollback(); // We only read data...
		return api_contest_round_create(contest_id, contest_perms);
	} else if (next_arg == "clone_round") {
		transaction.rollback(); // We only read data...
		return api_contest_round_clone(contest_id, contest_perms);
	} else if (not next_arg.empty()) {
		transaction.rollback(); // We only read data...
		return api_error400();
	}

	ContestInfoResponseBuilder resp_builder(resp.content, contest_overall_perms,
	                                        contest_perms, curr_date);
	resp_builder.append_field_names();
	resp_builder.append_contest(contest);

	sim::contest_round::iterate(
	   mysql, sim::contest_round::IterateIdKind::CONTEST, contest_id,
	   contest_perms, curr_date,
	   [&](const ContestRound& round) { resp_builder.append_round(round); });

	resp_builder.start_appending_problems();

	sim::contest_problem::iterate(
	   mysql, sim::contest_problem::IterateIdKind::CONTEST, contest_id,
	   contest_perms,
	   (session_is_open ? optional {strtoull(session_user_id)} : std::nullopt),
	   (session_is_open ? optional {session_user_type} : std::nullopt),
	   curr_date,
	   [&](const ContestProblem& contest_problem,
	       const sim::contest_problem::ExtraIterateData& extra_data) {
		   resp_builder.append_problem(contest_problem, extra_data);
	   });

	resp_builder.stop_appending_problems();
}

void Sim::api_contest_round(StringView contest_round_id) {
	STACK_UNWINDING_MARK;

	// We read data in several queries - transaction will make the data
	// consistent
	auto transaction = mysql.start_transaction();
	auto curr_date = mysql_date();

	auto contest_opt = sim::contest::get(
	   mysql, sim::contest::GetIdKind::CONTEST_ROUND, contest_round_id,
	   (session_is_open ? optional {session_user_id} : std::nullopt),
	   curr_date);
	if (not contest_opt)
		return api_error404();

	auto& [contest, contest_perms] = contest_opt.value();
	if (uint(~contest_perms & sim::contest::Permissions::VIEW))
		return api_error403();

	optional<ContestRound> contest_round_opt;
	sim::contest_round::iterate(
	   mysql, sim::contest_round::IterateIdKind::CONTEST_ROUND,
	   contest_round_id, contest_perms, curr_date,
	   [&](const ContestRound& cr) { contest_round_opt = cr; });

	if (not contest_round_opt) {
		return error500(); // Contest round have to exist after successful
		                   // sim::contest::get()
	}

	auto& contest_round = contest_round_opt.value();

	StringView next_arg = url_args.extractNextArg();
	if (next_arg == "ranking") {
		transaction.rollback(); // We only read data...
		return api_contest_ranking(contest_perms, "contest_round_id",
		                           contest_round_id);
	} else if (next_arg == "attach_problem") {
		transaction.rollback(); // We only read data...
		return api_contest_problem_add(contest.id, contest_round.id,
		                               contest_perms);
	} else if (next_arg == "edit") {
		transaction.rollback(); // We only read data...
		return api_contest_round_edit(contest_round.id, contest_perms);
	} else if (next_arg == "delete") {
		transaction.rollback(); // We only read data...
		return api_contest_round_delete(contest_round.id, contest_perms);
	} else if (not next_arg.empty()) {
		transaction.rollback(); // We only read data...
		return api_error404();
	}

	auto contest_overall_perms = sim::contest::get_overall_permissions(
	   (session_is_open ? optional(session_user_type) : std::nullopt));

	ContestInfoResponseBuilder resp_builder(resp.content, contest_overall_perms,
	                                        contest_perms, curr_date);
	resp_builder.append_field_names();
	resp_builder.append_contest(contest);
	resp_builder.append_round(contest_round);

	resp_builder.start_appending_problems();

	sim::contest_problem::iterate(
	   mysql, sim::contest_problem::IterateIdKind::CONTEST_ROUND,
	   contest_round_id, contest_perms,
	   (session_is_open ? optional {strtoull(session_user_id)} : std::nullopt),
	   (session_is_open ? optional {session_user_type} : std::nullopt),
	   curr_date,
	   [&](const ContestProblem& contest_problem,
	       const sim::contest_problem::ExtraIterateData& extra_data) {
		   resp_builder.append_problem(contest_problem, extra_data);
	   });

	resp_builder.stop_appending_problems();
}

void Sim::api_contest_problem(StringView contest_problem_id) {
	STACK_UNWINDING_MARK;

	// We read data in several queries - transaction will make the data
	// consistent
	auto transaction = mysql.start_transaction();
	auto curr_date = mysql_date();

	auto contest_opt = sim::contest::get(
	   mysql, sim::contest::GetIdKind::CONTEST_PROBLEM, contest_problem_id,
	   (session_is_open ? optional {session_user_id} : std::nullopt),
	   curr_date);
	if (not contest_opt)
		return api_error404();

	auto& [contest, contest_perms] = contest_opt.value();
	if (uint(~contest_perms & sim::contest::Permissions::VIEW))
		return api_error403();

	optional<pair<ContestProblem, sim::contest_problem::ExtraIterateData>>
	   contest_problem_info;
	sim::contest_problem::iterate(
	   mysql, sim::contest_problem::IterateIdKind::CONTEST_PROBLEM,
	   contest_problem_id, contest_perms,
	   (session_is_open ? optional {strtoull(session_user_id)} : std::nullopt),
	   (session_is_open ? optional {session_user_type} : std::nullopt),
	   curr_date,
	   [&](const ContestProblem& contest_problem,
	       const sim::contest_problem::ExtraIterateData& extra_data) {
		   contest_problem_info.emplace(contest_problem, extra_data);
	   });

	if (not contest_problem_info) {
		return error500(); // Contest problem have to exist after successful
		                   // sim::contest::get()
	}

	auto& [contest_problem, contest_problem_extra_data] =
	   contest_problem_info.value();

	auto problem_id_str = toString(contest_problem.problem_id);

	StringView next_arg = url_args.extractNextArg();
	if (next_arg == "statement") {
		transaction.rollback(); // We only read data...
		return api_contest_problem_statement(problem_id_str);
	} else if (next_arg == "ranking") {
		transaction.rollback(); // We only read data...
		return api_contest_ranking(contest_perms, "contest_problem_id",
		                           contest_problem_id);
	} else if (next_arg == "rejudge_all_submissions") {
		transaction.rollback(); // We only read data...
		return api_contest_problem_rejudge_all_submissions(
		   contest_problem_id, contest_perms, problem_id_str);
	} else if (next_arg == "edit") {
		transaction.rollback(); // We only read data...
		return api_contest_problem_edit(contest_problem_id, contest_perms);
	} else if (next_arg == "delete") {
		transaction.rollback(); // We only read data...
		return api_contest_problem_delete(contest_problem_id, contest_perms);
	} else if (not next_arg.empty()) {
		transaction.rollback(); // We only read data...
		return api_error404();
	}

	auto contest_overall_perms = sim::contest::get_overall_permissions(
	   (session_is_open ? optional(session_user_type) : std::nullopt));

	ContestInfoResponseBuilder resp_builder(resp.content, contest_overall_perms,
	                                        contest_perms, curr_date);
	resp_builder.append_field_names();
	resp_builder.append_contest(contest);

	sim::contest_round::iterate(
	   mysql, sim::contest_round::IterateIdKind::CONTEST_PROBLEM,
	   contest_problem_id, contest_perms, curr_date,
	   [&](const ContestRound& cr) { resp_builder.append_round(cr); });

	resp_builder.start_appending_problems();
	resp_builder.append_problem(contest_problem, contest_problem_extra_data);
	resp_builder.stop_appending_problems();
}

void Sim::api_contest_create(sim::contest::OverallPermissions overall_perms) {
	STACK_UNWINDING_MARK;
	using PERM = sim::contest::OverallPermissions;

	if (uint(~overall_perms & PERM::ADD_PRIVATE) and
	    uint(~overall_perms & PERM::ADD_PUBLIC)) {
		return api_error403();
	}

	// Validate fields
	StringView name;
	form_validate_not_blank(name, "name", "Contest's name",
	                        decltype(Contest::name)::max_len);

	bool is_public = request.form_data.exist("public");
	if (is_public and uint(~overall_perms & PERM::ADD_PUBLIC)) {
		add_notification("error",
		                 "You have no permissions to add a public contest");
	}

	if (not is_public and uint(~overall_perms & PERM::ADD_PRIVATE)) {
		add_notification("error",
		                 "You have no permissions to add a private contest");
	}

	if (notifications.size)
		return api_error400(notifications);

	auto transaction = mysql.start_transaction();

	// Add contest
	auto stmt = mysql.prepare("INSERT contests(name, is_public) VALUES(?, ?)");
	stmt.bindAndExecute(name, is_public);

	auto contest_id = stmt.insert_id();
	// Add user to owners
	mysql
	   .prepare("INSERT contest_users(user_id, contest_id, mode) "
	            "VALUES(?, ?, " CU_MODE_OWNER_STR ")")
	   .bindAndExecute(session_user_id, contest_id);

	transaction.commit();
	append(contest_id);
}

void Sim::api_contest_clone(sim::contest::OverallPermissions overall_perms) {
	STACK_UNWINDING_MARK;
	using PERM = sim::contest::OverallPermissions;

	if (uint(~overall_perms & PERM::ADD_PRIVATE) and
	    uint(~overall_perms & PERM::ADD_PUBLIC)) {
		return api_error403();
	}

	// Validate fields
	StringView name;
	form_validate_not_blank(name, "name", "Contest's name",
	                        decltype(Contest::name)::max_len);

	StringView source_contest_id;
	form_validate_not_blank(source_contest_id, "source_contest",
	                        "ID of the contest to clone");

	bool is_public = request.form_data.exist("public");
	if (is_public and uint(~overall_perms & PERM::ADD_PUBLIC)) {
		add_notification("error",
		                 "You have no permissions to add a public contest");
	}

	if (not is_public and uint(~overall_perms & PERM::ADD_PRIVATE)) {
		add_notification("error",
		                 "You have no permissions to add a private contest");
	}

	if (notifications.size)
		return api_error400(notifications);

	auto transaction = mysql.start_transaction();
	auto curr_date = mysql_date();

	auto source_contest_opt = sim::contest::get(
	   mysql, sim::contest::GetIdKind::CONTEST, source_contest_id,
	   (session_is_open ? optional {session_user_id} : std::nullopt),
	   curr_date);
	if (not source_contest_opt) {
		return api_error404(
		   "There is no contest with this id that you can clone");
	}

	auto& [source_contest, source_contest_perms] = source_contest_opt.value();
	if (uint(~source_contest_perms & sim::contest::Permissions::VIEW)) {
		return api_error404(
		   "There is no contest with this id that you can clone");
	}

	// Collect contest rounds to clone
	std::map<decltype(ContestRound::item), ContestRound> contest_rounds;
	sim::contest_round::iterate(
	   mysql, sim::contest_round::IterateIdKind::CONTEST, source_contest_id,
	   source_contest_perms, curr_date,
	   [&](const ContestRound& cr) { contest_rounds.emplace(cr.item, cr); });

	// Collect contest problems to clone
	std::map<pair<decltype(ContestRound::id), decltype(ContestProblem::item)>,
	         ContestProblem>
	   contest_problems;

	optional<ContestProblem> unclonable_contest_problem;
	sim::contest_problem::iterate(
	   mysql, sim::contest_problem::IterateIdKind::CONTEST, source_contest_id,
	   source_contest_perms,
	   (session_is_open ? optional {strtoull(session_user_id)} : std::nullopt),
	   (session_is_open ? optional {session_user_type} : std::nullopt),
	   curr_date,
	   [&](const ContestProblem& cp,
	       const sim::contest_problem::ExtraIterateData& extra_data) {
		   contest_problems.emplace(pair(cp.contest_round_id, cp.item), cp);
		   if (uint(~extra_data.problem_perms &
		            sim::problem::Permissions::VIEW)) {
			   unclonable_contest_problem = cp;
		   }
	   });

	if (unclonable_contest_problem) {
		return api_error403(intentionalUnsafeStringView(concat(
		   "You have no permissions to clone the contest problem with id ",
		   unclonable_contest_problem->id,
		   " because you have no permission to attach the problem with id ",
		   unclonable_contest_problem->problem_id, " to a contest round")));
	}

	// Add contest
	auto stmt = mysql.prepare("INSERT contests(name, is_public) VALUES(?, ?)");
	stmt.bindAndExecute(name, is_public);
	auto new_contest_id = stmt.insert_id();

	// Update contest rounds to fit into new contest
	{
		decltype(ContestRound::item) new_item = 0;
		for (auto& [item, cr] : contest_rounds) {
			cr.item = new_item++;
			cr.contest_id = new_contest_id;
		}
	}

	// Add contest rounds to the new contest
	std::map<decltype(ContestRound::id), decltype(ContestRound::id)>
	   old_round_id_to_new_id;
	stmt = mysql.prepare(
	   "INSERT contest_rounds(contest_id, name, item, begins, ends, "
	   "full_results, ranking_exposure) VALUES(?, ?, ?, ?, ?, ?, ?)");
	for (auto& [key, r] : contest_rounds) {
		stmt.bindAndExecute(r.contest_id, r.name, r.item, r.begins, r.ends,
		                    r.full_results, r.ranking_exposure);
		old_round_id_to_new_id.emplace(r.id, stmt.insert_id());
	}

	// Update contest problems to fit into the new contest
	{
		decltype(ContestProblem::item) new_item = 0;
		decltype(ContestRound::id) curr_round_id =
		   0; // Initial value does not matter
		for (auto& [key, cp] : contest_problems) {
			auto& [round_id, item] = key;
			if (round_id != curr_round_id) {
				curr_round_id = round_id;
				new_item = 0;
			}

			cp.item = new_item++;
			cp.contest_id = new_contest_id;
			cp.contest_round_id =
			   old_round_id_to_new_id.at(cp.contest_round_id);
		}
	}

	// Add contest problems to the new contest
	stmt = mysql.prepare(
	   "INSERT contest_problems(contest_round_id, contest_id, problem_id, name,"
	   " item, final_selecting_method, score_revealing) "
	   "VALUES(?, ?, ?, ?, ?, ?, ?)");
	for (auto& [key, cp] : contest_problems) {
		stmt.bindAndExecute(cp.contest_round_id, cp.contest_id, cp.problem_id,
		                    cp.name, cp.item, cp.final_selecting_method,
		                    cp.score_revealing);
	}

	// Add user to owners
	mysql
	   .prepare("INSERT contest_users(user_id, contest_id, mode) "
	            "VALUES(?, ?, " CU_MODE_OWNER_STR ")")
	   .bindAndExecute(session_user_id, new_contest_id);

	transaction.commit();
	append(new_contest_id);
}

void Sim::api_contest_edit(StringView contest_id,
                           sim::contest::Permissions perms, bool is_public) {
	STACK_UNWINDING_MARK;

	if (uint(~perms & sim::contest::Permissions::ADMIN))
		return api_error403();

	// Validate fields
	StringView name;
	form_validate_not_blank(name, "name", "Contest's name",
	                        decltype(Contest::name)::max_len);

	bool will_be_public = request.form_data.exist("public");
	if (will_be_public and not is_public and
	    uint(~perms & sim::contest::Permissions::MAKE_PUBLIC)) {
		add_notification("error",
		                 "You have no permissions to make this contest public");
	}

	bool make_submitters_contestants =
	   (not will_be_public and
	    request.form_data.exist("make_submitters_contestants"));

	if (notifications.size)
		return api_error400(notifications);

	auto transaction = mysql.start_transaction();
	// Update contest
	mysql.prepare("UPDATE contests SET name=?, is_public=? WHERE id=?")
	   .bindAndExecute(name, will_be_public, contest_id);

	if (make_submitters_contestants) {
		mysql
		   .prepare("INSERT IGNORE contest_users(user_id, contest_id, mode) "
		            "SELECT owner, ?, ? FROM submissions "
		            "WHERE contest_id=? GROUP BY owner")
		   .bindAndExecute(contest_id,
		                   EnumVal(sim::ContestUser::Mode::CONTESTANT),
		                   contest_id);
	}

	transaction.commit();
}

void Sim::api_contest_delete(StringView contest_id,
                             sim::contest::Permissions perms) {
	STACK_UNWINDING_MARK;

	if (uint(~perms & sim::contest::Permissions::DELETE))
		return api_error403();

	if (not check_submitted_password())
		return api_error403("Invalid password");

	// Queue deleting job
	auto stmt = mysql.prepare("INSERT jobs (creator, status, priority, type,"
	                          " added, aux_id, info, data) "
	                          "VALUES(?, " JSTATUS_PENDING_STR ", ?,"
	                          " " JTYPE_DELETE_CONTEST_STR ", ?, ?, '', '')");
	stmt.bindAndExecute(session_user_id, priority(JobType::DELETE_CONTEST),
	                    mysql_date(), contest_id);

	jobs::notify_job_server();
	append(stmt.insert_id());
}

void Sim::api_contest_round_create(StringView contest_id,
                                   sim::contest::Permissions perms) {
	STACK_UNWINDING_MARK;

	if (uint(~perms & sim::contest::Permissions::ADMIN))
		return api_error403();

	// Validate fields
	CStringView name, begins, ends, full_results, ranking_expo;
	form_validate_not_blank(name, "name", "Round's name",
	                        decltype(ContestRound::name)::max_len);
	form_validate_not_blank(begins, "begins", "Begin time",
	                        is_safe_inf_timestamp);
	form_validate_not_blank(ends, "ends", "End time", is_safe_inf_timestamp);
	form_validate_not_blank(full_results, "full_results", "Full results time",
	                        is_safe_inf_timestamp);
	form_validate_not_blank(ranking_expo, "ranking_expo", "Show ranking since",
	                        is_safe_inf_timestamp);

	if (notifications.size)
		return api_error400(notifications);

	// Add round
	auto curr_date = mysql_date();
	auto stmt = mysql.prepare("INSERT contest_rounds(contest_id, name, item,"
	                          " begins, ends, full_results, ranking_exposure) "
	                          "SELECT ?, ?, COALESCE(MAX(item)+1, 0), ?, ?, ?,"
	                          " ? "
	                          "FROM contest_rounds WHERE contest_id=?");
	stmt.bindAndExecute(
	   contest_id, name, inf_timestamp_to_InfDatetime(begins).to_str(),
	   inf_timestamp_to_InfDatetime(ends).to_str(),
	   inf_timestamp_to_InfDatetime(full_results).to_str(),
	   inf_timestamp_to_InfDatetime(ranking_expo).to_str(), contest_id);

	append(stmt.insert_id());
}

void Sim::api_contest_round_clone(StringView contest_id,
                                  sim::contest::Permissions perms) {
	STACK_UNWINDING_MARK;

	if (uint(~perms & sim::contest::Permissions::ADMIN))
		return api_error403();

	// Validate fields
	StringView source_contest_round_id;
	form_validate_not_blank(source_contest_round_id, "source_contest_round",
	                        "ID of the contest round to clone");

	CStringView name, begins, ends, full_results, ranking_expo;
	form_validate(name, "name", "Round's name",
	              decltype(ContestRound::name)::max_len);
	form_validate_not_blank(begins, "begins", "Begin time",
	                        is_safe_inf_timestamp);
	form_validate_not_blank(ends, "ends", "End time", is_safe_inf_timestamp);
	form_validate_not_blank(full_results, "full_results", "Full results time",
	                        is_safe_inf_timestamp);
	form_validate_not_blank(ranking_expo, "ranking_expo", "Show ranking since",
	                        is_safe_inf_timestamp);

	if (notifications.size)
		return api_error400(notifications);

	auto transaction = mysql.start_transaction();
	auto curr_date = mysql_date();

	auto source_contest_opt = sim::contest::get(
	   mysql, sim::contest::GetIdKind::CONTEST_ROUND, source_contest_round_id,
	   (session_is_open ? optional {session_user_id} : std::nullopt),
	   curr_date);
	if (not source_contest_opt) {
		return api_error404(
		   "There is no contest round with this id that you can clone");
	}

	auto& [source_contest, source_contest_perms] = source_contest_opt.value();
	if (uint(~source_contest_perms & sim::contest::Permissions::VIEW)) {
		return api_error404(
		   "There is no contest round with this id that you can clone");
	}

	optional<ContestRound> contest_round_opt;
	sim::contest_round::iterate(
	   mysql, sim::contest_round::IterateIdKind::CONTEST_ROUND,
	   source_contest_round_id, source_contest_perms, curr_date,
	   [&](const ContestRound& cr) { contest_round_opt = cr; });
	if (not contest_round_opt) {
		return error500(); // Contest round have to exist after successful
		                   // sim::contest::get()
	}

	auto& contest_round = contest_round_opt.value();
	contest_round.contest_id = strtoull(contest_id);
	if (not name.empty())
		contest_round.name = name;
	contest_round.begins = inf_timestamp_to_InfDatetime(begins);
	contest_round.ends = inf_timestamp_to_InfDatetime(ends);
	contest_round.full_results = inf_timestamp_to_InfDatetime(full_results);
	contest_round.ranking_exposure = inf_timestamp_to_InfDatetime(ranking_expo);

	// Collect contest problems to clone
	std::map<decltype(ContestProblem::item), ContestProblem> contest_problems;
	optional<ContestProblem> unclonable_contest_problem;
	sim::contest_problem::iterate(
	   mysql, sim::contest_problem::IterateIdKind::CONTEST_ROUND,
	   source_contest_round_id, source_contest_perms,
	   (session_is_open ? optional {strtoull(session_user_id)} : std::nullopt),
	   (session_is_open ? optional {session_user_type} : std::nullopt),
	   curr_date,
	   [&](const ContestProblem& cp,
	       const sim::contest_problem::ExtraIterateData& extra_data) {
		   contest_problems.emplace(cp.item, cp);
		   if (uint(~extra_data.problem_perms &
		            sim::problem::Permissions::VIEW)) {
			   unclonable_contest_problem = cp;
		   }
	   });

	if (unclonable_contest_problem) {
		return api_error403(intentionalUnsafeStringView(concat(
		   "You have no permissions to clone the contest problem with id ",
		   unclonable_contest_problem->id,
		   " because you have no permission to attach the problem with id ",
		   unclonable_contest_problem->problem_id, " to a contest round")));
	}

	// Add contest round
	auto stmt = mysql.prepare(
	   "INSERT contest_rounds(contest_id, name, item, begins, ends,"
	   " full_results, ranking_exposure) "
	   "SELECT ?, ?, COALESCE(MAX(item)+1, 0), ?, ?, ?, ? "
	   "FROM contest_rounds WHERE contest_id=?");
	stmt.bindAndExecute(contest_id, contest_round.name, contest_round.begins,
	                    contest_round.ends, contest_round.full_results,
	                    contest_round.ranking_exposure, contest_id);
	auto new_round_id = stmt.insert_id();

	// Update contest problems to fit into new contest round
	{
		decltype(ContestProblem::item) new_item = 0;
		for (auto& [item, cp] : contest_problems) {
			cp.item = new_item++;
			cp.contest_id = contest_round.contest_id;
			cp.contest_round_id = new_round_id;
		}
	}

	// Add contest problems to the new contest round
	stmt = mysql.prepare(
	   "INSERT contest_problems(contest_round_id, contest_id, problem_id, name,"
	   " item, final_selecting_method, score_revealing) "
	   "VALUES(?, ?, ?, ?, ?, ?, ?)");
	for (auto& [key, cp] : contest_problems) {
		stmt.bindAndExecute(cp.contest_round_id, cp.contest_id, cp.problem_id,
		                    cp.name, cp.item, cp.final_selecting_method,
		                    cp.score_revealing);
	}

	transaction.commit();
	append(new_round_id);
}

void Sim::api_contest_round_edit(uintmax_t contest_round_id,
                                 sim::contest::Permissions perms) {
	STACK_UNWINDING_MARK;

	if (uint(~perms & sim::contest::Permissions::ADMIN))
		return api_error403();

	// Validate fields
	CStringView name, begins, ends, full_results, ranking_expo;
	form_validate_not_blank(name, "name", "Round's name",
	                        decltype(ContestRound::name)::max_len);
	form_validate_not_blank(begins, "begins", "Begin time",
	                        is_safe_inf_timestamp);
	form_validate_not_blank(ends, "ends", "End time", is_safe_inf_timestamp);
	form_validate_not_blank(full_results, "full_results", "Full results time",
	                        is_safe_inf_timestamp);
	form_validate_not_blank(ranking_expo, "ranking_expo", "Show ranking since",
	                        is_safe_inf_timestamp);

	if (notifications.size)
		return api_error400(notifications);

	// Update round
	auto curr_date = mysql_date();
	auto stmt = mysql.prepare("UPDATE contest_rounds "
	                          "SET name=?, begins=?, ends=?, full_results=?,"
	                          " ranking_exposure=? "
	                          "WHERE id=?");
	stmt.bindAndExecute(name, inf_timestamp_to_InfDatetime(begins).to_str(),
	                    inf_timestamp_to_InfDatetime(ends).to_str(),
	                    inf_timestamp_to_InfDatetime(full_results).to_str(),
	                    inf_timestamp_to_InfDatetime(ranking_expo).to_str(),
	                    contest_round_id);
}

void Sim::api_contest_round_delete(uintmax_t contest_round_id,
                                   sim::contest::Permissions perms) {
	STACK_UNWINDING_MARK;

	if (uint(~perms & sim::contest::Permissions::ADMIN))
		return api_error403();

	if (not check_submitted_password())
		return api_error403("Invalid password");

	// Queue deleting job
	auto stmt = mysql.prepare("INSERT jobs (creator, status, priority, type,"
	                          " added, aux_id, info, data) "
	                          "VALUES(?, " JSTATUS_PENDING_STR ", ?,"
	                          " " JTYPE_DELETE_CONTEST_ROUND_STR ", ?, ?, '',"
	                          " '')");
	stmt.bindAndExecute(session_user_id,
	                    priority(JobType::DELETE_CONTEST_ROUND), mysql_date(),
	                    contest_round_id);

	jobs::notify_job_server();

	append(stmt.insert_id());
}

void Sim::api_contest_problem_add(uintmax_t contest_id,
                                  uintmax_t contest_round_id,
                                  sim::contest::Permissions perms) {
	STACK_UNWINDING_MARK;

	if (uint(~perms & sim::contest::Permissions::ADMIN))
		return api_error403();

	// Validate fields
	StringView name;
	StringView problem_id;
	static_assert(decltype(ContestProblem::name)::max_len >=
	                 decltype(Problem::name)::max_len,
	              "Contest problem name has to be able to hold the attached "
	              "problem's name");
	form_validate(name, "name", "Problem's name",
	              decltype(ContestProblem::name)::max_len);
	form_validate(problem_id, "problem_id", "Problem ID", isDigit,
	              "Problem ID: invalid value");
	// Validate score_revealing
	auto score_revealing_str = request.form_data.get("score_revealing");
	decltype(ContestProblem::score_revealing) score_revealing;
	if (score_revealing_str == "none") {
		score_revealing = ContestProblem::ScoreRevealingMode::NONE;
	} else if (score_revealing_str == "only_score") {
		score_revealing = ContestProblem::ScoreRevealingMode::ONLY_SCORE;
	} else if (score_revealing_str == "score_and_full_status") {
		score_revealing =
		   ContestProblem::ScoreRevealingMode::SCORE_AND_FULL_STATUS;
	} else {
		add_notification("error", "Invalid score revealing");
	}

	// Validate final_selecting_method
	auto fsm_str = request.form_data.get("final_selecting_method");
	decltype(ContestProblem::final_selecting_method) final_selecting_method;
	using FSSM = ContestProblem::FinalSubmissionSelectingMethod;
	if (fsm_str == "LC")
		final_selecting_method = FSSM::LAST_COMPILING;
	else if (fsm_str == "WHS")
		final_selecting_method = FSSM::WITH_HIGHEST_SCORE;
	else
		add_notification("error", "Invalid final selecting method");

	if (notifications.size)
		return api_error400(notifications);

	auto transaction = mysql.start_transaction();

	auto stmt =
	   mysql.prepare("SELECT owner, type, name FROM problems WHERE id=?");
	stmt.bindAndExecute(problem_id);

	MySQL::Optional<decltype(Problem::owner)::value_type> problem_owner;
	decltype(Problem::type) problem_type;
	decltype(ContestProblem::name) problem_name;
	stmt.res_bind_all(problem_owner, problem_type, problem_name);
	if (not stmt.next())
		return api_error404(intentionalUnsafeStringView(
		   concat("No problem was found with ID = ", problem_id)));

	auto problem_perms = sim::problem::get_permissions(
	   (session_is_open ? optional {strtoull(session_user_id)} : std::nullopt),
	   (session_is_open ? optional {session_user_type} : std::nullopt),
	   problem_owner, problem_type);
	if (uint(~problem_perms & sim::problem::Permissions::VIEW))
		return api_error403("You have no permissions to use this problem");

	// Add contest problem
	stmt = mysql.prepare("INSERT contest_problems(contest_round_id, contest_id,"
	                     " problem_id, name, item, final_selecting_method,"
	                     " score_revealing) "
	                     "SELECT ?, ?, ?, ?, COALESCE(MAX(item)+1, 0), ?, ? "
	                     "FROM contest_problems "
	                     "WHERE contest_round_id=?");
	stmt.bindAndExecute(contest_round_id, contest_id, problem_id,
	                    (name.empty() ? problem_name : name),
	                    final_selecting_method, score_revealing,
	                    contest_round_id);

	transaction.commit();
	append(stmt.insert_id());
}

void Sim::api_contest_problem_rejudge_all_submissions(
   StringView contest_problem_id, sim::contest::Permissions perms,
   StringView problem_id) {
	STACK_UNWINDING_MARK;

	if (uint(~perms & sim::contest::Permissions::ADMIN))
		return api_error403();

	mysql
	   .prepare("INSERT jobs (creator, status, priority, type,"
	            " added, aux_id, info, data) "
	            "SELECT ?, " JSTATUS_PENDING_STR ", ?,"
	            " " JTYPE_REJUDGE_SUBMISSION_STR ", ?, id, ?, '' "
	            "FROM submissions WHERE contest_problem_id=? ORDER BY id")
	   .bindAndExecute(session_user_id, priority(JobType::REJUDGE_SUBMISSION),
	                   mysql_date(), jobs::dumpString(problem_id),
	                   contest_problem_id);

	jobs::notify_job_server();
}

void Sim::api_contest_problem_edit(StringView contest_problem_id,
                                   sim::contest::Permissions perms) {
	STACK_UNWINDING_MARK;

	if (uint(~perms & sim::contest::Permissions::ADMIN))
		return api_error403();

	// Validate fields
	StringView name;
	form_validate_not_blank(name, "name", "Problem's name",
	                        decltype(ContestProblem::name)::max_len);

	auto score_revealing_str = request.form_data.get("score_revealing");
	decltype(ContestProblem::score_revealing) score_revealing;
	if (score_revealing_str == "none") {
		score_revealing = ContestProblem::ScoreRevealingMode::NONE;
	} else if (score_revealing_str == "only_score") {
		score_revealing = ContestProblem::ScoreRevealingMode::ONLY_SCORE;
	} else if (score_revealing_str == "score_and_full_status") {
		score_revealing =
		   ContestProblem::ScoreRevealingMode::SCORE_AND_FULL_STATUS;
	} else {
		add_notification("error", "Invalid score revealing");
	}

	// Validate final_selecting_method
	auto fsm_str = request.form_data.get("final_selecting_method");
	decltype(ContestProblem::final_selecting_method) final_selecting_method;
	using FSSM = ContestProblem::FinalSubmissionSelectingMethod;
	if (fsm_str == "LC")
		final_selecting_method = FSSM::LAST_COMPILING;
	else if (fsm_str == "WHS")
		final_selecting_method = FSSM::WITH_HIGHEST_SCORE;
	else
		add_notification("error", "Invalid final selecting method");

	if (notifications.size)
		return api_error400(notifications);

	// Have to check if it is necessary to reselect problem final submissions
	auto transaction = mysql.start_transaction();

	// Get the old final selecting method and whether the score was revealed
	auto stmt = mysql.prepare("SELECT final_selecting_method, score_revealing "
	                          "FROM contest_problems WHERE id=?");
	stmt.bindAndExecute(contest_problem_id);

	decltype(ContestProblem::final_selecting_method) old_final_selecting_method;
	decltype(ContestProblem::score_revealing) old_score_revealing;
	stmt.res_bind_all(old_final_selecting_method, old_score_revealing);
	if (not stmt.next())
		return; // Such contest problem does not exist (probably had just
		        // been deleted)

	bool reselect_final_sumbissions =
	   (old_final_selecting_method != final_selecting_method or
	    (final_selecting_method == FSSM::WITH_HIGHEST_SCORE and
	     score_revealing != old_score_revealing));

	if (reselect_final_sumbissions) {
		// Queue reselecting final submissions
		stmt = mysql.prepare(
		   "INSERT jobs (creator, status, priority, type, added,"
		   " aux_id, info, data) "
		   "VALUES(?, " JSTATUS_PENDING_STR
		   ", ?, " JTYPE_RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM_STR
		   ", ?, ?,"
		   " '', '')");
		stmt.bindAndExecute(
		   session_user_id,
		   priority(JobType::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM),
		   mysql_date(), contest_problem_id);

		append(stmt.insert_id());
	}

	// Update problem
	stmt =
	   mysql.prepare("UPDATE contest_problems "
	                 "SET name=?, score_revealing=?, final_selecting_method=? "
	                 "WHERE id=?");
	stmt.bindAndExecute(name, score_revealing, final_selecting_method,
	                    contest_problem_id);

	transaction.commit();
	jobs::notify_job_server();
}

void Sim::api_contest_problem_delete(StringView contest_problem_id,
                                     sim::contest::Permissions perms) {
	STACK_UNWINDING_MARK;

	if (uint(~perms & sim::contest::Permissions::ADMIN))
		return api_error403();

	if (not check_submitted_password())
		return api_error403("Invalid password");

	// Queue deleting job
	auto stmt = mysql.prepare("INSERT jobs (creator, status, priority, type,"
	                          " added, aux_id, info, data) "
	                          "VALUES(?, " JSTATUS_PENDING_STR ", ?,"
	                          " " JTYPE_DELETE_CONTEST_PROBLEM_STR ", ?, ?, '',"
	                          " '')");
	stmt.bindAndExecute(session_user_id,
	                    priority(JobType::DELETE_CONTEST_PROBLEM), mysql_date(),
	                    contest_problem_id);

	jobs::notify_job_server();
	append(stmt.insert_id());
}

void Sim::api_contest_problem_statement(StringView problem_id) {
	STACK_UNWINDING_MARK;

	auto stmt =
	   mysql.prepare("SELECT file_id, label, simfile FROM problems WHERE id=?");
	stmt.bindAndExecute(problem_id);

	decltype(Problem::file_id) problem_file_id;
	decltype(Problem::label) problem_label;
	decltype(Problem::simfile) problem_simfile;
	stmt.res_bind_all(problem_file_id, problem_label, problem_simfile);
	if (not stmt.next())
		return api_error404();

	return api_statement_impl(problem_file_id, problem_label, problem_simfile);
}

namespace {

struct SubmissionOwner {
	uint64_t id;
	InplaceBuff<32> name; // static size is set manually to save memory

	template <class... Args>
	SubmissionOwner(uint64_t sowner, Args&&... args) : id(sowner) {
		name.append(std::forward<Args>(args)...);
	}

	bool operator<(const SubmissionOwner& s) const noexcept {
		return id < s.id;
	}

	bool operator==(const SubmissionOwner& s) const noexcept {
		return id == s.id;
	}
};

} // anonymous namespace

void Sim::api_contest_ranking(sim::contest::Permissions perms,
                              StringView submissions_query_id_name,
                              StringView query_id) {
	STACK_UNWINDING_MARK;

	if (uint(~perms & sim::contest::Permissions::VIEW))
		return api_error403();

	// We read data several times, so transaction makes it consistent
	auto transaction = mysql.start_transaction();

	// Gather submissions owners
	auto stmt = mysql.prepare(intentionalUnsafeStringView(
	   concat("SELECT u.id, u.first_name, u.last_name FROM submissions s JOIN "
	          "users u ON s.owner=u.id WHERE s.",
	          submissions_query_id_name,
	          "=? AND s.contest_final=1 GROUP BY (u.id) ORDER BY u.id")));
	stmt.bindAndExecute(query_id);

	uintmax_t u_id;
	decltype(User::first_name) fname;
	decltype(User::last_name) lname;
	stmt.res_bind_all(u_id, fname, lname);

	std::vector<SubmissionOwner> sowners;
	while (stmt.next())
		sowners.emplace_back(u_id, fname, ' ', lname);

	// Gather submissions
	decltype(ContestRound::id) cr_id;
	decltype(ContestRound::full_results) cr_full_results;
	decltype(ContestProblem::id) cp_id;
	decltype(ContestProblem::score_revealing) cp_score_revealing;
	uintmax_t s_owner;
	uintmax_t sf_id;
	EnumVal<SubmissionStatus> sf_full_status;
	int64_t sf_score;
	uintmax_t si_id;
	EnumVal<SubmissionStatus> si_initial_status;

	// TODO: there is too much logic duplication (not only below) on whether to
	// show full or initial status and show or not show the score
	auto prepare_stmt = [&](auto&& extra_cr_sql, auto&&... extra_bind_params) {
		// clang-format off
		stmt = mysql.prepare(
		   "SELECT cr.id, cr.full_results, cp.id, cp.score_revealing, sf.owner,"
		   " sf.id, sf.full_status, sf.score, si.id, si.initial_status "
		   "FROM submissions sf "
		   "JOIN submissions si ON si.owner=sf.owner"
		   " AND si.contest_problem_id=sf.contest_problem_id"
		   " AND si.contest_initial_final=1 "
		   "JOIN contest_rounds cr ON cr.id=sf.contest_round_id ",
		      std::forward<decltype(extra_cr_sql)>(extra_cr_sql), " "
		   "JOIN contest_problems cp ON cp.id=sf.contest_problem_id "
		   "WHERE sf.", submissions_query_id_name, "=? AND sf.contest_final=1 "
		   "ORDER BY sf.owner");
		// clang-format on
		stmt.bindAndExecute(
		   std::forward<decltype(extra_bind_params)>(extra_bind_params)...,
		   query_id);
		stmt.res_bind_all(cr_id, cr_full_results, cp_id, cp_score_revealing,
		                  s_owner, sf_id, sf_full_status, sf_score, si_id,
		                  si_initial_status);
	};

	auto curr_date = mysql_date();

	bool is_admin = uint(perms & sim::contest::Permissions::ADMIN);
	if (is_admin) {
		prepare_stmt("");
	} else {
		prepare_stmt("AND cr.begins<=? AND cr.ranking_exposure<=?", curr_date,
		             curr_date);
	}

	append('[');
	// Column names
	// clang-format off
	append("\n{\"columns\":["
	           "\"id\",\"name\","
	           "{\"name\":\"submissions\",\"columns\":["
	               "\"id\",\"contest_round_id\","
	               "\"contest_problem_id\","
	               "{\"name\":\"status\",\"fields\":["
	                   "\"class\","
	                   "\"text\""
	               "]},"
	               "\"score\""
	           "]}"
	       "]}");
	// clang-format on

	const uint64_t session_uid =
	   (session_is_open ? strtoull(session_user_id) : 0);

	bool first_owner = true;
	std::optional<uintmax_t> prev_owner;
	bool show_owner_and_submission_id = false;

	while (stmt.next()) {
		// Owner changes
		if (first_owner or s_owner != prev_owner.value()) {
			auto it = binaryFind(sowners, SubmissionOwner(s_owner));
			if (it == sowners.end())
				continue; // Ignore submission as there is no owner to bind it
				          // to (this maybe a little race condition, but if the
				          // user will query again it will not be the case (with
				          // the current owner))

			if (first_owner) {
				append(",\n[");
				first_owner = false;
			} else {
				--resp.content.size; // remove trailing ','
				append("\n]],[");
			}

			prev_owner = s_owner;
			show_owner_and_submission_id =
			   (is_admin or (session_is_open and session_uid == s_owner));
			// Owner
			if (show_owner_and_submission_id)
				append(s_owner);
			else
				append("null");
			// Owner name
			append(',', jsonStringify(it->name), ",[");
		}

		bool show_full_status = whether_to_show_full_status(
		   perms, cr_full_results, curr_date, cp_score_revealing);

		append("\n[");
		if (not show_owner_and_submission_id) {
			append("null,");
		} else if (show_full_status) {
			append(sf_id, ',');
		} else {
			append(si_id, ',');
		}

		append(cr_id, ',', cp_id, ',');
		append_submission_status(si_initial_status, sf_full_status,
		                         show_full_status);

		bool show_score = whether_to_show_score(perms, cr_full_results,
		                                        curr_date, cp_score_revealing);
		if (show_score) {
			append(',', sf_score, "],");
		} else {
			append(",null],");
		}
	}

	if (first_owner) // no submission was appended
		append(']');
	else {
		--resp.content.size; // remove trailing ','
		append("\n]]]");
	}
}

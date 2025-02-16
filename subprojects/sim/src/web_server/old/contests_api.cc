#include "../capabilities/contests.hh"
#include "../http/form_validation.hh"
#include "sim.hh"

#include <cstdint>
#include <map>
#include <sim/contest_problems/contest_problem.hh>
#include <sim/contest_problems/iterate.hh>
#include <sim/contest_problems/old_contest_problem.hh>
#include <sim/contest_rounds/iterate.hh>
#include <sim/contest_rounds/old_contest_round.hh>
#include <sim/contest_users/old_contest_user.hh>
#include <sim/contests/get.hh>
#include <sim/contests/old_contest.hh>
#include <sim/contests/permissions.hh>
#include <sim/inf_datetime.hh>
#include <sim/job_server/notify.hh>
#include <sim/mysql/mysql.hh>
#include <sim/submissions/old_submission.hh>
#include <simlib/from_unsafe.hh>
#include <simlib/inplace_buff.hh>
#include <simlib/string_view.hh>
#include <utility>

using sim::inf_timestamp_to_InfDatetime;
using sim::InfDatetime;
using sim::is_safe_inf_timestamp;
using sim::contest_problems::ContestProblem;
using sim::contest_problems::OldContestProblem;
using sim::contest_rounds::OldContestRound;
using sim::contest_users::OldContestUser;
using sim::contests::OldContest;
using sim::jobs::OldJob;
using sim::problems::OldProblem;
using sim::submissions::OldSubmission;
using sim::users::User;
using std::map;
using std::optional;
using std::pair;

namespace web_server::old {

inline bool whether_to_show_full_status(
    sim::contests::Permissions cperms,
    const InfDatetime& full_results,
    const decltype(utc_mysql_datetime())& curr_mysql_date,
    OldContestProblem::ScoreRevealing score_revealing
) {
    // TODO: check append_submission_status() for making it use the new function
    if (uint(cperms & sim::contests::Permissions::ADMIN) or full_results <= curr_mysql_date) {
        return true;
    }

    switch (score_revealing) {
    case OldContestProblem::ScoreRevealing::NONE:
    case OldContestProblem::ScoreRevealing::ONLY_SCORE: return false;
    case OldContestProblem::ScoreRevealing::SCORE_AND_FULL_STATUS: return true;
    }

    __builtin_unreachable();
}

inline bool whether_to_show_score(
    sim::contests::Permissions cperms,
    const InfDatetime& full_results,
    const decltype(utc_mysql_datetime())& curr_mysql_date,
    OldContestProblem::ScoreRevealing score_revealing
) {
    // TODO: check append_submission_status() for making it use the new function
    if (uint(cperms & sim::contests::Permissions::ADMIN)) {
        return true;
    }

    if (full_results <= curr_mysql_date) {
        return true;
    }

    switch (score_revealing) {
    case OldContestProblem::ScoreRevealing::NONE: return false;
    case OldContestProblem::ScoreRevealing::ONLY_SCORE:
    case OldContestProblem::ScoreRevealing::SCORE_AND_FULL_STATUS: return true;
    }

    __builtin_unreachable();
}

static inline InplaceBuff<32> color_class_json(
    sim::contests::Permissions cperms,
    const InfDatetime& full_results,
    const decltype(utc_mysql_datetime())& curr_mysql_date,
    optional<OldSubmission::Status> full_status,
    optional<OldSubmission::Status> initial_status,
    OldContestProblem::ScoreRevealing score_revealing
) {

    if (whether_to_show_full_status(cperms, full_results, curr_mysql_date, score_revealing)) {
        if (full_status.has_value()) {
            return concat<32>("\"", css_color_class(full_status.value()), "\"");
        }
    } else if (initial_status.has_value()) {
        return concat<32>("\"initial ", css_color_class(initial_status.value()), "\"");
    }

    return concat<32>("null");
}

template <class T>
static void append_contest_actions_str(
    T& str, capabilities::Contests caps_contests, sim::contests::Permissions perms
) {
    STACK_UNWINDING_MARK;
    using P = sim::contests::Permissions;

    str.append('"');
    if (caps_contests.add_public or caps_contests.add_private) {
        str.append('a');
    }
    if (uint(perms & P::MAKE_PUBLIC)) {
        str.append('M');
    }
    if (uint(perms & P::VIEW)) {
        str.append('v');
    }
    if (uint(perms & P::PARTICIPATE)) {
        str.append('p');
    }
    if (uint(perms & P::ADMIN)) {
        str.append('A');
    }
    if (uint(perms & P::DELETE)) {
        str.append('D');
    }
    str.append('"');
}

namespace {

class ContestInfoResponseBuilder {
    using ContentType = decltype(http::Response::content)&;
    ContentType& content_;
    capabilities::Contests caps_contests_;
    sim::contests::Permissions contest_perms_;
    std::string curr_date_;
    std::map<uint64_t, InfDatetime> round_to_full_results_;

public:
    ContestInfoResponseBuilder(
        ContentType& resp_content,
        capabilities::Contests caps_contests,
        sim::contests::Permissions contest_perms,
        std::string curr_date
    )
    : content_(resp_content)
    , caps_contests_(caps_contests)
    , contest_perms_(contest_perms)
    , curr_date_(std::move(curr_date)) {}

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
                      "\"method_of_choosing_final_submission\","
                      "\"score_revealing\","
                      "\"color_class\""
                  "]}"
              "]},");
        // clang-format on
    }

    void append_contest(const OldContest& contest) {
        content_.append(
            "\n[",
            contest.id,
            ',',
            json_stringify(contest.name),
            (contest.is_public ? ",true," : ",false,")
        );
        append_contest_actions_str(content_, caps_contests_, contest_perms_);

        content_.append("],\n["); // TODO: this is ugly...
    }

    void append_round(const OldContestRound& contest_round) {
        round_to_full_results_.emplace(contest_round.id, contest_round.full_results);
        // clang-format off
        content_.append(
           "\n[", contest_round.id, ',',
           json_stringify(contest_round.name), ',',
           contest_round.item, ","
           "\"", contest_round.ranking_exposure.as_inf_datetime().to_api_str(), "\","
           "\"", contest_round.begins.as_inf_datetime().to_api_str(), "\","
           "\"", contest_round.full_results.as_inf_datetime().to_api_str(), "\","
           "\"", contest_round.ends.as_inf_datetime().to_api_str(), "\"],");
        // clang-format on
    }

    void start_appending_problems() {
        if (content_.back() == ',') {
            --content_.size;
        }
        content_.append("\n],\n[");
    }

    void append_problem(
        const OldContestProblem& contest_problem,
        const sim::contest_problems::ExtraIterateData& extra_data
    ) {
        auto& cp = contest_problem;
        // clang-format off
        content_.append(
            "\n[", cp.id, ',',
            cp.contest_round_id, ',',
            cp.problem_id, ',',
            (uint(extra_data.problem_perms & sim::problems::Permissions::VIEW) ? "true,"
                                                                              : "false,"),
            json_stringify(extra_data.problem_label), ',',
            json_stringify(cp.name), ',',
            cp.item, ',',
            cp.method_of_choosing_final_submission.to_enum().to_quoted_str(), ',',
            cp.score_revealing.to_enum().to_quoted_str(), ',',
            color_class_json(
                contest_perms_, round_to_full_results_[cp.contest_round_id],
                curr_date_, extra_data.final_submission_full_status,
                extra_data.initial_final_submission_initial_status,
                cp.score_revealing),
            "],");
        // clang-format on
    }

    void stop_appending_problems() {
        if (content_.back() == ',') {
            --content_.size;
        }
        content_.append("\n]\n]");
    }
};

} // namespace

void Sim::api_contests() {
    STACK_UNWINDING_MARK;

    // We may read data several times (permission checking), so transaction is
    // used to ensure data consistency
    auto transaction = mysql.start_repeatable_read_transaction();

    InplaceBuff<512> qfields;
    InplaceBuff<512> qwhere;
    qfields.append("SELECT c.id, c.name, c.is_public, cu.mode");
    qwhere.append(
        " FROM contests c LEFT JOIN contest_users cu ON "
        "cu.contest_id=c.id AND cu.user_id=",
        (session.has_value() ? from_unsafe{to_string(session->user_id)} : StringView("''"))
    );

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
    auto caps_contests = capabilities::contests_for(session);
    // Choose contests to select
    if (not caps_contests.view_all) {
        if (caps_contests.view_public) {
            if (session.has_value()) {
                qwhere_append("(c.is_public=1 OR cu.mode IS NOT NULL)");
            } else {
                qwhere_append("c.is_public=1");
            }
        } else {
            if (session.has_value()) {
                qwhere_append("cu.mode IS NOT NULL");
            } else {
                qwhere_append("0=1");
            }
        }
    }

    // Process restrictions
    auto rows_limit = API_FIRST_QUERY_ROWS_LIMIT;
    StringView next_arg = url_args.extract_next_arg();
    for (uint mask = 0; !next_arg.empty(); next_arg = url_args.extract_next_arg()) {
        constexpr uint ID_COND = 1;
        constexpr uint PUBLIC_COND = 2;

        auto arg = decode_uri(next_arg);
        char cond = arg[0];
        StringView arg_id = StringView(arg).substr(1);

        if (cond == 'p' and ~mask & PUBLIC_COND) { // Is public
            if (arg_id == "Y") {
                qwhere_append("c.is_public=1");
            } else if (arg_id == "N") {
                qwhere_append("c.is_public=0");
            } else {
                return api_error400(from_unsafe{concat("Invalid is_public condition: ", arg_id)});
            }

            mask |= PUBLIC_COND;

            // NOLINTNEXTLINE(bugprone-branch-clone)
        } else if (not is_digit(arg_id)) {
            return api_error400();

        } else if (is_one_of(cond, '=', '<', '>') and ~mask & ID_COND) { // conditional
            rows_limit = API_OTHER_QUERY_ROWS_LIMIT;
            qwhere_append("c.id", arg);
            mask |= ID_COND;

        } else {
            return api_error400();
        }
    }

    // Execute query
    qfields.append(qwhere, " ORDER BY c.id DESC LIMIT ", rows_limit);
    auto old_mysql = old_mysql::ConnectionView{mysql};
    auto res = old_mysql.query(qfields);

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
        bool is_public = WONT_THROW(str2num<bool>(res[IS_PUBLIC]).value());
        auto cumode =
            (res.is_null(USER_MODE)
                 ? std::nullopt
                 : optional(OldContestUser::Mode(WONT_THROW(
                       str2num<OldContestUser::Mode::UnderlyingType>(res[USER_MODE]).value()
                   ))));
        auto contest_perms = sim::contests::get_permissions(
            (session.has_value() ? optional(session->user_type) : std::nullopt), is_public, cumode
        );

        // Id
        append(",\n[", cid, ",");
        // Name
        append(json_stringify(name), ',');
        // Is public
        append(is_public ? "true," : "false,");

        // Append what buttons to show
        append_contest_actions_str(resp.content, caps_contests, contest_perms);
        append(']');
    }

    append("\n]");
}

void Sim::api_contest() {
    STACK_UNWINDING_MARK;

    auto caps_contests = capabilities::contests_for(session);
    StringView next_arg = url_args.extract_next_arg();
    if (next_arg == "create") {
        return api_contest_create(caps_contests);
    }
    if (next_arg == "clone") {
        return api_contest_clone(caps_contests);
    }
    if (next_arg.empty()) {
        return api_error400();
    }
    if (next_arg[0] == 'r' and is_digit(next_arg.substr(1))) {
        // Select by contest round id
        return api_contest_round(next_arg.substr(1));
    }
    if (next_arg[0] == 'p' and is_digit(next_arg.substr(1))) {
        // Select by contest problem id
        return api_contest_problem(next_arg.substr(1));
    }
    if (not(next_arg[0] == 'c' and is_digit(next_arg.substr(1)))) {
        return api_error404();
    }

    StringView contest_id = next_arg.substr(1);

    // We read data in several queries - transaction will make the data
    // consistent
    auto transaction = mysql.start_repeatable_read_transaction();
    auto curr_date = utc_mysql_datetime();

    auto contest_opt = sim::contests::get(
        mysql,
        sim::contests::GetIdKind::CONTEST,
        contest_id,
        (session.has_value() ? optional{session->user_id} : std::nullopt),
        curr_date
    );
    if (not contest_opt) {
        return api_error404();
    }

    auto& [contest, contest_perms] = contest_opt.value();
    if (uint(~contest_perms & sim::contests::Permissions::VIEW)) {
        return api_error403(); // Could not participate
    }

    next_arg = url_args.extract_next_arg();
    if (next_arg == "ranking") {
        transaction.rollback(); // We only read data...
        return api_contest_ranking(contest_perms, "contest_id", contest_id);
    }
    if (next_arg == "edit") {
        transaction.rollback(); // We only read data...
        return api_contest_edit(contest_id, contest_perms, contest.is_public);
    }
    if (next_arg == "delete") {
        transaction.rollback(); // We only read data...
        return api_contest_delete(contest_id, contest_perms);
    }
    if (next_arg == "create_round") {
        transaction.rollback(); // We only read data...
        return api_contest_round_create(contest_id, contest_perms);
    }
    if (next_arg == "clone_round") {
        transaction.rollback(); // We only read data...
        return api_contest_round_clone(contest_id, contest_perms);
    }
    if (not next_arg.empty()) {
        transaction.rollback(); // We only read data...
        return api_error400();
    }

    ContestInfoResponseBuilder resp_builder(resp.content, caps_contests, contest_perms, curr_date);
    resp_builder.append_field_names();
    resp_builder.append_contest(contest);

    sim::contest_rounds::iterate(
        mysql,
        sim::contest_rounds::IterateIdKind::CONTEST,
        contest_id,
        contest_perms,
        curr_date,
        [&](const OldContestRound& round) { resp_builder.append_round(round); }
    );

    resp_builder.start_appending_problems();

    sim::contest_problems::iterate(
        mysql,
        sim::contest_problems::IterateIdKind::CONTEST,
        contest_id,
        contest_perms,
        (session.has_value() ? optional{session->user_id} : std::nullopt),
        (session.has_value() ? optional{session->user_type} : std::nullopt),
        curr_date,
        [&](const OldContestProblem& contest_problem,
            const sim::contest_problems::ExtraIterateData& extra_data) {
            resp_builder.append_problem(contest_problem, extra_data);
        }
    );

    resp_builder.stop_appending_problems();
}

void Sim::api_contest_round(StringView contest_round_id) {
    STACK_UNWINDING_MARK;

    // We read data in several queries - transaction will make the data
    // consistent
    auto transaction = mysql.start_repeatable_read_transaction();
    auto curr_date = utc_mysql_datetime();

    auto contest_opt = sim::contests::get(
        mysql,
        sim::contests::GetIdKind::CONTEST_ROUND,
        contest_round_id,
        (session.has_value() ? optional{session->user_id} : std::nullopt),
        curr_date
    );
    if (not contest_opt) {
        return api_error404();
    }

    auto& [contest, contest_perms] = contest_opt.value();
    if (uint(~contest_perms & sim::contests::Permissions::VIEW)) {
        return api_error403();
    }

    optional<OldContestRound> contest_round_opt;
    sim::contest_rounds::iterate(
        mysql,
        sim::contest_rounds::IterateIdKind::CONTEST_ROUND,
        contest_round_id,
        contest_perms,
        curr_date,
        [&](const OldContestRound& cr) { contest_round_opt = cr; }
    );

    if (not contest_round_opt) {
        return error500(); // Contest round have to exist after successful sim::contests::get()
    }

    auto& contest_round = contest_round_opt.value();

    StringView next_arg = url_args.extract_next_arg();
    if (next_arg == "ranking") {
        transaction.rollback(); // We only read data...
        return api_contest_ranking(contest_perms, "contest_round_id", contest_round_id);
    }
    if (next_arg == "attach_problem") {
        transaction.rollback(); // We only read data...
        return api_contest_problem_add(contest.id, contest_round.id, contest_perms);
    }
    if (next_arg == "edit") {
        transaction.rollback(); // We only read data...
        return api_contest_round_edit(contest_round.id, contest_perms);
    }
    if (next_arg == "delete") {
        transaction.rollback(); // We only read data...
        return api_contest_round_delete(contest_round.id, contest_perms);
    }
    if (not next_arg.empty()) {
        transaction.rollback(); // We only read data...
        return api_error404();
    }

    auto caps_contests = capabilities::contests_for(session);
    ContestInfoResponseBuilder resp_builder(resp.content, caps_contests, contest_perms, curr_date);
    resp_builder.append_field_names();
    resp_builder.append_contest(contest);
    resp_builder.append_round(contest_round);

    resp_builder.start_appending_problems();

    sim::contest_problems::iterate(
        mysql,
        sim::contest_problems::IterateIdKind::CONTEST_ROUND,
        contest_round_id,
        contest_perms,
        (session.has_value() ? optional{session->user_id} : std::nullopt),
        (session.has_value() ? optional{session->user_type} : std::nullopt),
        curr_date,
        [&](const OldContestProblem& contest_problem,
            const sim::contest_problems::ExtraIterateData& extra_data) {
            resp_builder.append_problem(contest_problem, extra_data);
        }
    );

    resp_builder.stop_appending_problems();
}

void Sim::api_contest_problem(StringView contest_problem_id) {
    STACK_UNWINDING_MARK;

    // We read data in several queries - transaction will make the data
    // consistent
    auto transaction = mysql.start_repeatable_read_transaction();
    auto curr_date = utc_mysql_datetime();

    auto contest_opt = sim::contests::get(
        mysql,
        sim::contests::GetIdKind::CONTEST_PROBLEM,
        contest_problem_id,
        (session.has_value() ? optional{session->user_id} : std::nullopt),
        curr_date
    );
    if (not contest_opt) {
        return api_error404();
    }

    auto& [contest, contest_perms] = contest_opt.value();
    if (uint(~contest_perms & sim::contests::Permissions::VIEW)) {
        return api_error403();
    }

    optional<pair<OldContestProblem, sim::contest_problems::ExtraIterateData>> contest_problem_info;
    sim::contest_problems::iterate(
        mysql,
        sim::contest_problems::IterateIdKind::CONTEST_PROBLEM,
        contest_problem_id,
        contest_perms,
        (session.has_value() ? optional{session->user_id} : std::nullopt),
        (session.has_value() ? optional{session->user_type} : std::nullopt),
        curr_date,
        [&](const OldContestProblem& contest_problem,
            const sim::contest_problems::ExtraIterateData& extra_data) {
            contest_problem_info.emplace(contest_problem, extra_data);
        }
    );

    if (not contest_problem_info) {
        return error500(); // Contest problem have to exist after successful
                           // sim::contests::get()
    }

    auto& [contest_problem, contest_problem_extra_data] = contest_problem_info.value();

    auto problem_id_str = to_string(contest_problem.problem_id);

    StringView next_arg = url_args.extract_next_arg();
    if (next_arg == "statement") {
        transaction.rollback(); // We only read data...
        return api_contest_problem_statement(problem_id_str);
    }
    if (next_arg == "ranking") {
        transaction.rollback(); // We only read data...
        return api_contest_ranking(contest_perms, "contest_problem_id", contest_problem_id);
    }
    if (next_arg == "rejudge_all_submissions") {
        transaction.rollback(); // We only read data...
        return api_contest_problem_rejudge_all_submissions(contest_problem_id, contest_perms);
    }
    if (next_arg == "edit") {
        transaction.rollback(); // We only read data...
        return api_contest_problem_edit(contest_problem_id, contest_perms);
    }
    if (next_arg == "delete") {
        transaction.rollback(); // We only read data...
        return api_contest_problem_delete(contest_problem_id, contest_perms);
    }
    if (not next_arg.empty()) {
        transaction.rollback(); // We only read data...
        return api_error404();
    }

    auto caps_contests = capabilities::contests_for(session);
    ContestInfoResponseBuilder resp_builder(resp.content, caps_contests, contest_perms, curr_date);
    resp_builder.append_field_names();
    resp_builder.append_contest(contest);

    sim::contest_rounds::iterate(
        mysql,
        sim::contest_rounds::IterateIdKind::CONTEST_PROBLEM,
        contest_problem_id,
        contest_perms,
        curr_date,
        [&](const OldContestRound& cr) { resp_builder.append_round(cr); }
    );

    resp_builder.start_appending_problems();
    resp_builder.append_problem(contest_problem, contest_problem_extra_data);
    resp_builder.stop_appending_problems();
}

void Sim::api_contest_create(capabilities::Contests caps_contests) {
    STACK_UNWINDING_MARK;
    if (not caps_contests.add_private and not caps_contests.add_public) {
        return api_error403();
    }

    // Validate fields
    StringView name;
    form_validate_not_blank(name, "name", "Contest's name", decltype(OldContest::name)::max_len);

    bool is_public = request.form_fields.contains("public");
    if (is_public and !caps_contests.add_public) {
        add_notification("error", "You have no permissions to add a public contest");
    }

    if (not is_public and !caps_contests.add_private) {
        add_notification("error", "You have no permissions to add a private contest");
    }

    if (notifications.size) {
        return api_error400(notifications);
    }

    auto transaction = mysql.start_repeatable_read_transaction();

    // Add contest
    auto old_mysql = old_mysql::ConnectionView{mysql};
    auto stmt = old_mysql.prepare("INSERT contests(created_at, name, is_public) VALUES(?, ?, ?)");
    stmt.bind_and_execute(utc_mysql_datetime(), name, is_public);

    auto contest_id = stmt.insert_id();
    // Add user to owners
    old_mysql
        .prepare("INSERT contest_users(user_id, contest_id, mode) "
                 "VALUES(?, ?, ?)")
        .bind_and_execute(session->user_id, contest_id, EnumVal(OldContestUser::Mode::OWNER));

    transaction.commit();
    append(contest_id);
}

void Sim::api_contest_clone(capabilities::Contests caps_contests) {
    STACK_UNWINDING_MARK;

    if (not caps_contests.add_private and not caps_contests.add_public) {
        return api_error403();
    }

    // Validate fields
    StringView name;
    form_validate_not_blank(name, "name", "Contest's name", decltype(OldContest::name)::max_len);

    StringView source_contest_id;
    form_validate_not_blank(source_contest_id, "source_contest", "ID of the contest to clone");

    bool is_public = request.form_fields.contains("public");
    if (is_public and !caps_contests.add_public) {
        add_notification("error", "You have no permissions to add a public contest");
    }

    if (not is_public and !caps_contests.add_private) {
        add_notification("error", "You have no permissions to add a private contest");
    }

    if (notifications.size) {
        return api_error400(notifications);
    }

    auto transaction = mysql.start_repeatable_read_transaction();
    auto curr_date = utc_mysql_datetime();

    auto source_contest_opt = sim::contests::get(
        mysql,
        sim::contests::GetIdKind::CONTEST,
        source_contest_id,
        (session.has_value() ? optional{session->user_id} : std::nullopt),
        curr_date
    );
    if (not source_contest_opt) {
        return api_error404("There is no contest with this id that you can clone");
    }

    auto& [source_contest, source_contest_perms] = source_contest_opt.value();
    if (uint(~source_contest_perms & sim::contests::Permissions::VIEW)) {
        return api_error404("There is no contest with this id that you can clone");
    }

    // Collect contest rounds to clone
    std::map<decltype(OldContestRound::item), OldContestRound> contest_rounds;
    sim::contest_rounds::iterate(
        mysql,
        sim::contest_rounds::IterateIdKind::CONTEST,
        source_contest_id,
        source_contest_perms,
        curr_date,
        [&](const OldContestRound& cr) { contest_rounds.emplace(cr.item, cr); }
    );

    // Collect contest problems to clone
    std::map<
        pair<decltype(OldContestRound::id), decltype(OldContestProblem::item)>,
        OldContestProblem>
        contest_problems;

    optional<OldContestProblem> unclonable_contest_problem;
    sim::contest_problems::iterate(
        mysql,
        sim::contest_problems::IterateIdKind::CONTEST,
        source_contest_id,
        source_contest_perms,
        (session.has_value() ? optional{session->user_id} : std::nullopt),
        (session.has_value() ? optional{session->user_type} : std::nullopt),
        curr_date,
        [&](const OldContestProblem& cp,
            const sim::contest_problems::ExtraIterateData& extra_data) {
            contest_problems.emplace(pair(cp.contest_round_id, cp.item), cp);
            if (uint(~extra_data.problem_perms & sim::problems::Permissions::VIEW)) {
                unclonable_contest_problem = cp;
            }
        }
    );

    if (unclonable_contest_problem) {
        return api_error403(from_unsafe{concat(
            "You have no permissions to clone the contest problem with id ",
            unclonable_contest_problem->id,
            " because you have no permission to attach the problem with id ",
            unclonable_contest_problem->problem_id,
            " to a contest round"
        )});
    }

    // Add contest
    auto old_mysql = old_mysql::ConnectionView{mysql};
    auto stmt = old_mysql.prepare("INSERT contests(created_at, name, is_public) VALUES(?, ?, ?)");
    stmt.bind_and_execute(utc_mysql_datetime(), name, is_public);
    auto new_contest_id = stmt.insert_id();

    // Update contest rounds to fit into new contest
    {
        decltype(OldContestRound::item) new_item = 0;
        for (auto& [item, cr] : contest_rounds) {
            cr.item = new_item++;
            cr.contest_id = new_contest_id;
        }
    }

    // Add contest rounds to the new contest
    std::map<decltype(OldContestRound::id), decltype(OldContestRound::id)> old_round_id_to_new_id;
    stmt =
        old_mysql.prepare("INSERT contest_rounds(created_at, contest_id, name, item, begins, ends, "
                          "full_results, ranking_exposure) VALUES(?, ?, ?, ?, ?, ?, ?, ?)");
    for (auto& [key, r] : contest_rounds) {
        stmt.bind_and_execute(
            utc_mysql_datetime(),
            r.contest_id,
            r.name,
            r.item,
            r.begins,
            r.ends,
            r.full_results,
            r.ranking_exposure
        );
        old_round_id_to_new_id.emplace(r.id, stmt.insert_id());
    }

    // Update contest problems to fit into the new contest
    {
        decltype(OldContestProblem::item) new_item = 0;
        decltype(OldContestRound::id) curr_round_id = 0; // Initial value does not matter
        for (auto& [key, cp] : contest_problems) {
            auto& [round_id, item] = key;
            if (round_id != curr_round_id) {
                curr_round_id = round_id;
                new_item = 0;
            }

            cp.item = new_item++;
            cp.contest_id = new_contest_id;
            cp.contest_round_id = old_round_id_to_new_id.at(cp.contest_round_id);
        }
    }

    // Add contest problems to the new contest
    stmt = old_mysql.prepare(
        "INSERT contest_problems(created_at, contest_round_id, contest_id, problem_id, name,"
        " item, method_of_choosing_final_submission, score_revealing) "
        "VALUES(?,?, ?, ?, ?, ?, ?, ?)"
    );
    for (auto& [key, cp] : contest_problems) {
        stmt.bind_and_execute(
            utc_mysql_datetime(),
            cp.contest_round_id,
            cp.contest_id,
            cp.problem_id,
            cp.name,
            cp.item,
            cp.method_of_choosing_final_submission,
            cp.score_revealing
        );
    }

    // Add user to owners
    old_mysql
        .prepare("INSERT contest_users(user_id, contest_id, mode) "
                 "VALUES(?, ?, ?)")
        .bind_and_execute(session->user_id, new_contest_id, EnumVal(OldContestUser::Mode::OWNER));

    transaction.commit();
    append(new_contest_id);
}

void
Sim::api_contest_edit(StringView contest_id, sim::contests::Permissions perms, bool is_public) {
    STACK_UNWINDING_MARK;

    if (uint(~perms & sim::contests::Permissions::ADMIN)) {
        return api_error403();
    }

    // Validate fields
    StringView name;
    form_validate_not_blank(name, "name", "Contest's name", decltype(OldContest::name)::max_len);

    bool will_be_public = request.form_fields.contains("public");
    if (will_be_public and not is_public and uint(~perms & sim::contests::Permissions::MAKE_PUBLIC))
    {
        add_notification("error", "You have no permissions to make this contest public");
    }

    bool make_submitters_contestants =
        (not will_be_public and request.form_fields.contains("make_submitters_contestants"));

    if (notifications.size) {
        return api_error400(notifications);
    }

    auto transaction = mysql.start_repeatable_read_transaction();
    // Update contest
    auto old_mysql = old_mysql::ConnectionView{mysql};
    old_mysql.prepare("UPDATE contests SET name=?, is_public=? WHERE id=?")
        .bind_and_execute(name, will_be_public, contest_id);

    if (make_submitters_contestants) {
        old_mysql
            .prepare("INSERT IGNORE contest_users(user_id, contest_id, mode) "
                     "SELECT user_id, ?, ? FROM submissions "
                     "WHERE contest_id=? GROUP BY user_id")
            .bind_and_execute(contest_id, EnumVal(OldContestUser::Mode::CONTESTANT), contest_id);
    }

    transaction.commit();
}

void Sim::api_contest_delete(StringView contest_id, sim::contests::Permissions perms) {
    STACK_UNWINDING_MARK;

    if (uint(~perms & sim::contests::Permissions::DELETE)) {
        return api_error403();
    }

    if (not check_submitted_password()) {
        return api_error403("Invalid password");
    }

    // Queue deleting job
    auto old_mysql = old_mysql::ConnectionView{mysql};
    auto stmt =
        old_mysql.prepare("INSERT jobs (creator, status, priority, type, created_at, aux_id) "
                          "VALUES(?, ?, ?, ?, ?, ?)");
    stmt.bind_and_execute(
        session->user_id,
        EnumVal(OldJob::Status::PENDING),
        default_priority(OldJob::Type::DELETE_CONTEST),
        EnumVal(OldJob::Type::DELETE_CONTEST),
        utc_mysql_datetime(),
        contest_id
    );

    sim::job_server::notify_job_server();
    append(stmt.insert_id());
}

void Sim::api_contest_round_create(StringView contest_id, sim::contests::Permissions perms) {
    STACK_UNWINDING_MARK;

    if (uint(~perms & sim::contests::Permissions::ADMIN)) {
        return api_error403();
    }

    // Validate fields
    CStringView name;
    CStringView begins;
    CStringView ends;
    CStringView full_results;
    CStringView ranking_expo;
    form_validate_not_blank(name, "name", "Round's name", decltype(OldContestRound::name)::max_len);
    form_validate_not_blank(begins, "begins", "Begin time", is_safe_inf_timestamp);
    form_validate_not_blank(ends, "ends", "End time", is_safe_inf_timestamp);
    form_validate_not_blank(
        full_results, "full_results", "Full results time", is_safe_inf_timestamp
    );
    form_validate_not_blank(
        ranking_expo, "ranking_expo", "Show ranking since", is_safe_inf_timestamp
    );

    if (notifications.size) {
        return api_error400(notifications);
    }

    // Add round
    auto curr_date = utc_mysql_datetime();
    auto old_mysql = old_mysql::ConnectionView{mysql};
    auto stmt = old_mysql.prepare("INSERT contest_rounds(created_at, contest_id, name, item,"
                                  " begins, ends, full_results, ranking_exposure) "
                                  "SELECT ?, ?, ?, COALESCE(MAX(item)+1, 0), ?, ?, ?, ? "
                                  "FROM contest_rounds WHERE contest_id=?");
    stmt.bind_and_execute(
        utc_mysql_datetime(),
        contest_id,
        name,
        inf_timestamp_to_InfDatetime(begins).to_str(),
        inf_timestamp_to_InfDatetime(ends).to_str(),
        inf_timestamp_to_InfDatetime(full_results).to_str(),
        inf_timestamp_to_InfDatetime(ranking_expo).to_str(),
        contest_id
    );

    append(stmt.insert_id());
}

void Sim::api_contest_round_clone(StringView contest_id, sim::contests::Permissions perms) {
    STACK_UNWINDING_MARK;

    if (uint(~perms & sim::contests::Permissions::ADMIN)) {
        return api_error403();
    }

    // Validate fields
    StringView source_contest_round_id;
    form_validate_not_blank(
        source_contest_round_id, "source_contest_round", "ID of the contest round to clone"
    );

    CStringView name;
    CStringView begins;
    CStringView ends;
    CStringView full_results;
    CStringView ranking_expo;
    form_validate(name, "name", "Round's name", decltype(OldContestRound::name)::max_len);
    form_validate_not_blank(begins, "begins", "Begin time", is_safe_inf_timestamp);
    form_validate_not_blank(ends, "ends", "End time", is_safe_inf_timestamp);
    form_validate_not_blank(
        full_results, "full_results", "Full results time", is_safe_inf_timestamp
    );
    form_validate_not_blank(
        ranking_expo, "ranking_expo", "Show ranking since", is_safe_inf_timestamp
    );

    if (notifications.size) {
        return api_error400(notifications);
    }

    auto transaction = mysql.start_repeatable_read_transaction();
    auto curr_date = utc_mysql_datetime();

    auto source_contest_opt = sim::contests::get(
        mysql,
        sim::contests::GetIdKind::CONTEST_ROUND,
        source_contest_round_id,
        (session.has_value() ? optional{session->user_id} : std::nullopt),
        curr_date
    );
    if (not source_contest_opt) {
        return api_error404("There is no contest round with this id that you can clone");
    }

    auto& [source_contest, source_contest_perms] = source_contest_opt.value();
    if (uint(~source_contest_perms & sim::contests::Permissions::VIEW)) {
        return api_error404("There is no contest round with this id that you can clone");
    }

    optional<OldContestRound> contest_round_opt;
    sim::contest_rounds::iterate(
        mysql,
        sim::contest_rounds::IterateIdKind::CONTEST_ROUND,
        source_contest_round_id,
        source_contest_perms,
        curr_date,
        [&](const OldContestRound& cr) { contest_round_opt = cr; }
    );
    if (not contest_round_opt) {
        return error500(); // Contest round have to exist after successful sim::contests::get()
    }

    auto& contest_round = contest_round_opt.value();
    contest_round.contest_id =
        WONT_THROW(str2num<decltype(contest_round.contest_id)>(contest_id).value());
    if (not name.empty()) {
        contest_round.name = name;
    }
    contest_round.begins = inf_timestamp_to_InfDatetime(begins);
    contest_round.ends = inf_timestamp_to_InfDatetime(ends);
    contest_round.full_results = inf_timestamp_to_InfDatetime(full_results);
    contest_round.ranking_exposure = inf_timestamp_to_InfDatetime(ranking_expo);

    // Collect contest problems to clone
    std::map<decltype(OldContestProblem::item), OldContestProblem> contest_problems;
    optional<OldContestProblem> unclonable_contest_problem;
    sim::contest_problems::iterate(
        mysql,
        sim::contest_problems::IterateIdKind::CONTEST_ROUND,
        source_contest_round_id,
        source_contest_perms,
        (session.has_value() ? optional{session->user_id} : std::nullopt),
        (session.has_value() ? optional{session->user_type} : std::nullopt),
        curr_date,
        [&](const OldContestProblem& cp,
            const sim::contest_problems::ExtraIterateData& extra_data) {
            contest_problems.emplace(cp.item, cp);
            if (uint(~extra_data.problem_perms & sim::problems::Permissions::VIEW)) {
                unclonable_contest_problem = cp;
            }
        }
    );

    if (unclonable_contest_problem) {
        return api_error403(from_unsafe{concat(
            "You have no permissions to clone the contest problem with id ",
            unclonable_contest_problem->id,
            " because you have no permission to attach the problem with id ",
            unclonable_contest_problem->problem_id,
            " to a contest round"
        )});
    }

    // Add contest round
    auto old_mysql = old_mysql::ConnectionView{mysql};
    auto stmt =
        old_mysql.prepare("INSERT contest_rounds(created_at, contest_id, name, item, begins, ends,"
                          " full_results, ranking_exposure) "
                          "SELECT ?, ?, ?, COALESCE(MAX(item)+1, 0), ?, ?, ?, ? "
                          "FROM contest_rounds WHERE contest_id=?");
    stmt.bind_and_execute(
        utc_mysql_datetime(),
        contest_id,
        contest_round.name,
        contest_round.begins,
        contest_round.ends,
        contest_round.full_results,
        contest_round.ranking_exposure,
        contest_id
    );
    auto new_round_id = stmt.insert_id();

    // Update contest problems to fit into new contest round
    {
        decltype(OldContestProblem::item) new_item = 0;
        for (auto& [item, cp] : contest_problems) {
            cp.item = new_item++;
            cp.contest_id = contest_round.contest_id;
            cp.contest_round_id = new_round_id;
        }
    }

    // Add contest problems to the new contest round
    stmt = old_mysql.prepare(
        "INSERT contest_problems(created_at, contest_round_id, contest_id, problem_id, name,"
        " item, method_of_choosing_final_submission, score_revealing) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?)"
    );
    for (auto& [key, cp] : contest_problems) {
        stmt.bind_and_execute(
            utc_mysql_datetime(),
            cp.contest_round_id,
            cp.contest_id,
            cp.problem_id,
            cp.name,
            cp.item,
            cp.method_of_choosing_final_submission,
            cp.score_revealing
        );
    }

    transaction.commit();
    append(new_round_id);
}

void Sim::api_contest_round_edit(
    decltype(OldContestRound::id) contest_round_id, sim::contests::Permissions perms
) {
    STACK_UNWINDING_MARK;

    if (uint(~perms & sim::contests::Permissions::ADMIN)) {
        return api_error403();
    }

    // Validate fields
    CStringView name;
    CStringView begins;
    CStringView ends;
    CStringView full_results;
    CStringView ranking_expo;
    form_validate_not_blank(name, "name", "Round's name", decltype(OldContestRound::name)::max_len);
    form_validate_not_blank(begins, "begins", "Begin time", is_safe_inf_timestamp);
    form_validate_not_blank(ends, "ends", "End time", is_safe_inf_timestamp);
    form_validate_not_blank(
        full_results, "full_results", "Full results time", is_safe_inf_timestamp
    );
    form_validate_not_blank(
        ranking_expo, "ranking_expo", "Show ranking since", is_safe_inf_timestamp
    );

    if (notifications.size) {
        return api_error400(notifications);
    }

    // Update round
    auto curr_date = utc_mysql_datetime();
    auto old_mysql = old_mysql::ConnectionView{mysql};
    auto stmt = old_mysql.prepare("UPDATE contest_rounds "
                                  "SET name=?, begins=?, ends=?, full_results=?,"
                                  " ranking_exposure=? "
                                  "WHERE id=?");
    stmt.bind_and_execute(
        name,
        inf_timestamp_to_InfDatetime(begins).to_str(),
        inf_timestamp_to_InfDatetime(ends).to_str(),
        inf_timestamp_to_InfDatetime(full_results).to_str(),
        inf_timestamp_to_InfDatetime(ranking_expo).to_str(),
        contest_round_id
    );
}

void Sim::api_contest_round_delete(
    decltype(OldContestRound::id) contest_round_id, sim::contests::Permissions perms
) {
    STACK_UNWINDING_MARK;

    if (uint(~perms & sim::contests::Permissions::ADMIN)) {
        return api_error403();
    }

    if (not check_submitted_password()) {
        return api_error403("Invalid password");
    }

    // Queue deleting job
    auto old_mysql = old_mysql::ConnectionView{mysql};
    auto stmt =
        old_mysql.prepare("INSERT jobs (creator, status, priority, type, created_at, aux_id) "
                          "VALUES(?, ?, ?, ?, ?, ?)");
    stmt.bind_and_execute(
        session->user_id,
        EnumVal(OldJob::Status::PENDING),
        default_priority(OldJob::Type::DELETE_CONTEST_ROUND),
        EnumVal(OldJob::Type::DELETE_CONTEST_ROUND),
        utc_mysql_datetime(),
        contest_round_id
    );

    sim::job_server::notify_job_server();

    append(stmt.insert_id());
}

namespace params {

constexpr http::ApiParam contest_problem_problem_id{
    &ContestProblem::problem_id, "problem_id", "Problem ID"
};
constexpr http::ApiParam contest_problem_name{&ContestProblem::name, "name", "Problem's name"};
constexpr http::ApiParam contest_problem_method_of_choosing_final_submission{
    &ContestProblem::method_of_choosing_final_submission,
    "method_of_choosing_final_submission",
    "Method of choosing final submission"
};
constexpr http::ApiParam contest_problem_score_revealing{
    &ContestProblem::score_revealing, "score_revealing", "Score revealing"
};

} // namespace params

void Sim::api_contest_problem_add(
    decltype(OldContest::id) contest_id,
    decltype(OldContestRound::id) contest_round_id,
    sim::contests::Permissions perms
) {
    STACK_UNWINDING_MARK;

    if (uint(~perms & sim::contests::Permissions::ADMIN)) {
        return api_error403();
    }

    VALIDATE(request.form_fields, api_error400,
        (problem_id, params::contest_problem_problem_id, REQUIRED)
        (name, allow_blank(params::contest_problem_name), REQUIRED)
        (method_of_choosing_final_submission, params::contest_problem_method_of_choosing_final_submission, REQUIRED)
        (score_revealing, params::contest_problem_score_revealing, REQUIRED)
    );

    auto transaction = mysql.start_repeatable_read_transaction();

    auto old_mysql = old_mysql::ConnectionView{mysql};
    auto stmt = old_mysql.prepare("SELECT owner_id, visibility, name FROM problems WHERE id=?");
    stmt.bind_and_execute(problem_id);

    old_mysql::Optional<decltype(OldProblem::owner_id)::value_type> problem_owner_id;
    decltype(OldProblem::visibility) problem_visibility;
    decltype(OldProblem::name) problem_name;
    stmt.res_bind_all(problem_owner_id, problem_visibility, problem_name);
    if (not stmt.next()) {
        return api_error404(from_unsafe{concat("No problem was found with ID = ", problem_id)});
    }

    auto problem_perms = sim::problems::get_permissions(
        (session.has_value() ? optional{session->user_id} : std::nullopt),
        (session.has_value() ? optional{session->user_type} : std::nullopt),
        problem_owner_id,
        problem_visibility
    );
    if (uint(~problem_perms & sim::problems::Permissions::VIEW)) {
        return api_error403("You have no permissions to use this problem");
    }

    // Add contest problem
    stmt = old_mysql.prepare("INSERT contest_problems(created_at, contest_round_id, contest_id,"
                             " problem_id, name, item, method_of_choosing_final_submission,"
                             " score_revealing) "
                             "SELECT ?, ?, ?, ?, ?, COALESCE(MAX(item)+1, 0), ?, ? "
                             "FROM contest_problems "
                             "WHERE contest_round_id=?");
    static_assert( // NOLINTNEXTLINE(misc-redundant-expression)
        decltype(OldContestProblem::name)::max_len >= decltype(OldProblem::name)::max_len,
        "Contest problem name has to be able to hold the attached problem's name"
    );
    stmt.bind_and_execute(
        utc_mysql_datetime(),
        contest_round_id,
        contest_id,
        problem_id,
        (name.empty() ? StringView{problem_name} : StringView{name}),
        EnumVal{method_of_choosing_final_submission},
        EnumVal{score_revealing},
        contest_round_id
    );

    transaction.commit();
    append(stmt.insert_id());
}

void Sim::api_contest_problem_rejudge_all_submissions(
    StringView contest_problem_id, sim::contests::Permissions perms
) {
    STACK_UNWINDING_MARK;

    if (uint(~perms & sim::contests::Permissions::ADMIN)) {
        return api_error403();
    }

    auto old_mysql = old_mysql::ConnectionView{mysql};
    old_mysql
        .prepare("INSERT jobs (creator, status, priority, type, created_at, aux_id) "
                 "SELECT ?, ?, ?, ?, ?, id "
                 "FROM submissions WHERE contest_problem_id=? ORDER BY id")
        .bind_and_execute(
            session->user_id,
            EnumVal(OldJob::Status::PENDING),
            default_priority(OldJob::Type::REJUDGE_SUBMISSION),
            EnumVal(OldJob::Type::REJUDGE_SUBMISSION),
            utc_mysql_datetime(),
            contest_problem_id
        );

    sim::job_server::notify_job_server();
}

void
Sim::api_contest_problem_edit(StringView contest_problem_id, sim::contests::Permissions perms) {
    STACK_UNWINDING_MARK;

    if (uint(~perms & sim::contests::Permissions::ADMIN)) {
        return api_error403();
    }

    VALIDATE(request.form_fields, api_error400,
        (name, params::contest_problem_name, REQUIRED)
        (method_of_choosing_final_submission, params::contest_problem_method_of_choosing_final_submission, REQUIRED)
        (score_revealing, params::contest_problem_score_revealing, REQUIRED)
    );

    // Have to check if it is necessary to reselect problem final submissions
    auto transaction = mysql.start_repeatable_read_transaction();

    // Get the old method of choosing final submission and whether the score was revealed
    auto stmt =
        mysql.execute(sim::sql::Select("method_of_choosing_final_submission, score_revealing")
                          .from("contest_problems")
                          .where("id=?", contest_problem_id));

    decltype(ContestProblem::method_of_choosing_final_submission
    ) old_method_of_choosing_final_submission;
    decltype(ContestProblem::score_revealing) old_score_revealing;
    stmt.res_bind(old_method_of_choosing_final_submission, old_score_revealing);
    if (not stmt.next()) {
        return; // Such contest problem does not exist (probably had just
                // been deleted)
    }

    bool reselect_final_sumbissions =
        (old_method_of_choosing_final_submission != method_of_choosing_final_submission or
         (method_of_choosing_final_submission ==
              ContestProblem::MethodOfChoosingFinalSubmission::HIGHEST_SCORE and
          score_revealing != old_score_revealing));

    auto old_mysql = old_mysql::ConnectionView{mysql};
    if (reselect_final_sumbissions) {
        // Queue reselecting final submissions

        old_mysql
            .prepare("INSERT jobs (creator, status, priority, type, created_at, aux_id) "
                     "VALUES(?, ?, ?, ?, ?, ?)")
            .bind_and_execute(
                session->user_id,
                EnumVal(OldJob::Status::PENDING),
                default_priority(OldJob::Type::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM),
                EnumVal(OldJob::Type::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM),
                utc_mysql_datetime(),
                contest_problem_id
            );

        append(old_mysql.insert_id());
    }

    // Update problem
    old_mysql
        .prepare("UPDATE contest_problems "
                 "SET name=?, score_revealing=?, method_of_choosing_final_submission=? "
                 "WHERE id=?")
        .bind_and_execute(
            name,
            EnumVal{score_revealing},
            EnumVal{method_of_choosing_final_submission},
            contest_problem_id
        );

    transaction.commit();
    sim::job_server::notify_job_server();
}

void
Sim::api_contest_problem_delete(StringView contest_problem_id, sim::contests::Permissions perms) {
    STACK_UNWINDING_MARK;

    if (uint(~perms & sim::contests::Permissions::ADMIN)) {
        return api_error403();
    }

    if (not check_submitted_password()) {
        return api_error403("Invalid password");
    }

    // Queue deleting job

    auto old_mysql = old_mysql::ConnectionView{mysql};
    auto stmt =
        old_mysql.prepare("INSERT jobs (creator, status, priority, type, created_at, aux_id) "
                          "VALUES(?, ?, ?, ?, ?, ?)");
    stmt.bind_and_execute(
        session->user_id,
        EnumVal(OldJob::Status::PENDING),
        default_priority(OldJob::Type::DELETE_CONTEST_PROBLEM),
        EnumVal(OldJob::Type::DELETE_CONTEST_PROBLEM),
        utc_mysql_datetime(),
        contest_problem_id
    );

    sim::job_server::notify_job_server();
    append(stmt.insert_id());
}

void Sim::api_contest_problem_statement(StringView problem_id) {
    STACK_UNWINDING_MARK;

    auto old_mysql = old_mysql::ConnectionView{mysql};
    auto stmt = old_mysql.prepare("SELECT file_id, label, simfile FROM problems WHERE id=?");
    stmt.bind_and_execute(problem_id);

    decltype(OldProblem::file_id) problem_file_id = 0;
    decltype(OldProblem::label) problem_label;
    decltype(OldProblem::simfile) problem_simfile;
    stmt.res_bind_all(problem_file_id, problem_label, problem_simfile);
    if (not stmt.next()) {
        return api_error404();
    }

    return api_statement_impl(problem_file_id, problem_label, problem_simfile);
}

namespace {

struct SubmissionUser {
    uint64_t id;
    InplaceBuff<32> name; // static size is set manually to save memory

    template <class... Args>
    explicit SubmissionUser(uint64_t suser_id, Args&&... args) : id(suser_id) {
        name.append(std::forward<Args>(args)...);
    }

    bool operator<(const SubmissionUser& s) const noexcept { return id < s.id; }

    bool operator==(const SubmissionUser& s) const noexcept { return id == s.id; }
};

} // anonymous namespace

void Sim::api_contest_ranking(
    sim::contests::Permissions perms, StringView submissions_query_id_name, StringView query_id
) {
    STACK_UNWINDING_MARK;

    if (uint(~perms & sim::contests::Permissions::VIEW)) {
        return api_error403();
    }

    // We read data several times, so transaction makes it consistent
    auto transaction = mysql.start_repeatable_read_transaction();

    // Gather submissions owners
    auto old_mysql = old_mysql::ConnectionView{mysql};
    auto stmt = old_mysql.prepare(
        "SELECT u.id, u.first_name, u.last_name FROM submissions s JOIN "
        "users u ON s.user_id=u.id WHERE s.",
        submissions_query_id_name,
        "=? AND s.contest_problem_final=1 GROUP BY (u.id) ORDER BY u.id"
    );
    stmt.bind_and_execute(query_id);

    decltype(User::id) u_id = 0;
    InplaceBuff<0> fname;
    InplaceBuff<0> lname;
    stmt.res_bind_all(u_id, fname, lname);

    std::vector<SubmissionUser> susers;
    while (stmt.next()) {
        susers.emplace_back(u_id, fname, ' ', lname);
    }

    // Gather submissions
    decltype(OldContestRound::id) cr_id = 0;
    decltype(OldContestRound::full_results) cr_full_results;
    decltype(OldContestProblem::id) cp_id = 0;
    decltype(OldContestProblem::score_revealing) cp_score_revealing;
    decltype(OldSubmission::user_id)::value_type s_user_id = 0;
    decltype(OldSubmission::id) sf_id = 0;
    EnumVal<OldSubmission::Status> sf_full_status{};
    int64_t sf_score = 0;
    decltype(OldSubmission::id) si_id = 0;
    EnumVal<OldSubmission::Status> si_initial_status{};

    // TODO: there is too much logic duplication (not only below) on whether to
    // show full or initial status and show or not show the score
    auto prepare_stmt = [&](auto&& extra_cr_sql, auto&&... extra_bind_params) {
        // clang-format off
        stmt = old_mysql.prepare(
           "SELECT cr.id, cr.full_results, cp.id, cp.score_revealing, sf.user_id,"
           " sf.id, sf.full_status, sf.score, si.id, si.initial_status "
           "FROM submissions sf "
           "JOIN submissions si ON si.user_id=sf.user_id"
           " AND si.contest_problem_id=sf.contest_problem_id"
           " AND si.contest_problem_initial_final=1 "
           "JOIN contest_rounds cr ON cr.id=sf.contest_round_id ",
              std::forward<decltype(extra_cr_sql)>(extra_cr_sql), " "
           "JOIN contest_problems cp ON cp.id=sf.contest_problem_id "
           "WHERE sf.", submissions_query_id_name, "=? AND sf.contest_problem_final=1 "
           "ORDER BY sf.user_id");
        // clang-format on
        stmt.bind_and_execute(
            std::forward<decltype(extra_bind_params)>(extra_bind_params)..., query_id
        );
        stmt.res_bind_all(
            cr_id,
            cr_full_results,
            cp_id,
            cp_score_revealing,
            s_user_id,
            sf_id,
            sf_full_status,
            sf_score,
            si_id,
            si_initial_status
        );
    };

    auto curr_date = utc_mysql_datetime();

    bool is_admin = uint(perms & sim::contests::Permissions::ADMIN);
    if (is_admin) {
        prepare_stmt("");
    } else {
        prepare_stmt("AND cr.begins<=? AND cr.ranking_exposure<=?", curr_date, curr_date);
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

    const uint64_t session_uid = (session.has_value() ? session->user_id : 0);

    bool first_user = true;
    std::optional<decltype(s_user_id)> prev_user;
    bool show_user_and_submission_id = false;

    while (stmt.next()) {
        // Owner changes
        if (first_user or s_user_id != prev_user.value()) {
            auto it = binary_find(susers, SubmissionUser(s_user_id));
            if (it == susers.end()) {
                continue; // Ignore submission as there is no user_id to bind it
                          // to (this maybe a little race condition, but if the
                          // user will query again it will not be the case (with
                          // the current user_id))
            }

            if (first_user) {
                append(",\n[");
                first_user = false;
            } else {
                --resp.content.size; // remove trailing ','
                append("\n]],[");
            }

            prev_user = s_user_id;
            show_user_and_submission_id =
                (is_admin or (session.has_value() and session_uid == s_user_id));
            // Owner
            if (show_user_and_submission_id) {
                append(s_user_id);
            } else {
                append("null");
            }
            // Owner name
            append(',', json_stringify(it->name), ",[");
        }

        bool show_full_status =
            whether_to_show_full_status(perms, cr_full_results, curr_date, cp_score_revealing);

        append("\n[");
        if (not show_user_and_submission_id) {
            append("null,");
        } else if (show_full_status) {
            append(sf_id, ',');
        } else {
            append(si_id, ',');
        }

        append(cr_id, ',', cp_id, ',');
        append_submission_status(si_initial_status, sf_full_status, show_full_status);

        bool show_score =
            whether_to_show_score(perms, cr_full_results, curr_date, cp_score_revealing);
        if (show_score) {
            append(',', sf_score, "],");
        } else {
            append(",null],");
        }
    }

    if (first_user) { // no submission was appended
        append(']');
    } else {
        --resp.content.size; // remove trailing ','
        append("\n]]]");
    }
}

} // namespace web_server::old

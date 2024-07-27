#include "sim.hh"

#include <functional>
#include <optional>
#include <sim/contest_problems/contest_problem.hh>
#include <sim/contest_problems/old_contest_problem.hh>
#include <sim/contests/old_contest.hh>
#include <sim/inf_datetime.hh>
#include <sim/job_server/notify.hh>
#include <sim/sql/sql.hh>
#include <sim/submissions/old_submission.hh>
#include <sim/submissions/submission.hh>
#include <sim/submissions/update_final.hh>
#include <simlib/call_in_destructor.hh>
#include <simlib/file_contents.hh>
#include <simlib/file_info.hh>
#include <simlib/file_manip.hh>
#include <simlib/from_unsafe.hh>
#include <simlib/humanize.hh>
#include <simlib/string_view.hh>
#include <simlib/time.hh>

using sim::InfDatetime;
using sim::contest_problems::OldContestProblem;
using sim::contest_users::OldContestUser;
using sim::jobs::OldJob;
using sim::problems::OldProblem;
using sim::submissions::OldSubmission;
using sim::users::User;
using std::optional;
using std::string;

namespace web_server::old {

void Sim::append_submission_status(
    OldSubmission::Status initial_status, OldSubmission::Status full_status, bool show_full_status
) {
    STACK_UNWINDING_MARK;
    using SS = OldSubmission::Status;

    auto as_str = [](OldSubmission::Status status) {
        switch (status) {
        case SS::OK: return R"(","OK"])";
        case SS::WA: return R"(","Wrong answer"])";
        case SS::TLE: return R"(","Time limit exceeded"])";
        case SS::MLE: return R"(","Memory limit exceeded"])";
        case SS::OLE: return R"(","Output size limit exceeded"])";
        case SS::RTE: return R"(","Runtime error"])";
        case SS::PENDING: return R"(","Pending"])";
        case SS::COMPILATION_ERROR: return R"(","Compilation failed"])";
        case SS::CHECKER_COMPILATION_ERROR: return R"(","Checker compilation failed"])";
        case SS::JUDGE_ERROR: return R"(","Judge error"])";
        }

        return R"(","Unknown"])"; // Shouldn't happen
    };

    if (show_full_status) {
        append("[\"", css_color_class(full_status), as_str(full_status));
    } else {
        append("[\"initial ", css_color_class(initial_status), as_str(initial_status));
    }
}

void Sim::api_submissions() {
    STACK_UNWINDING_MARK;
    using PERM = SubmissionPermissions;

    if (not session.has_value()) {
        return api_error403();
    }

    // We may read data several times (permission checking), so transaction is
    // used to ensure data consistency
    auto transaction = mysql.start_repeatable_read_transaction();

    InplaceBuff<512> qfields;
    InplaceBuff<512> qwhere;
    qfields.append("SELECT s.id, s.type, s.language, s.user_id, cu.mode, p.owner_id,"
                   " u.username, u.first_name, u.last_name, s.problem_id,"
                   " p.name, s.contest_problem_id, cp.name,"
                   " cp.method_of_choosing_final_submission, cp.score_revealing,"
                   " s.contest_round_id, r.name, r.full_results, r.ends,"
                   " s.contest_id, c.name, s.created_at, s.problem_final,"
                   " s.contest_problem_final, s.contest_problem_initial_final,"
                   " s.initial_status, s.full_status, s.score");
    qwhere.append(
        " FROM submissions s "
        "LEFT JOIN users u ON u.id=s.user_id "
        "STRAIGHT_JOIN problems p ON p.id=s.problem_id "
        "LEFT JOIN contest_problems cp ON cp.id=s.contest_problem_id "
        "LEFT JOIN contest_rounds r ON r.id=s.contest_round_id "
        "LEFT JOIN contests c ON c.id=s.contest_id "
        "LEFT JOIN contest_users cu ON cu.contest_id=s.contest_id"
        " AND cu.user_id=",
        session->user_id,
        " WHERE TRUE"
    ); // Needed to easily append constraints

    enum ColumnIdx {
        SID,
        STYPE,
        SLANGUAGE,
        SUSER,
        CUMODE,
        POWNER_ID,
        SUSER_USERNAME,
        SUSER_FNAME,
        SUSER_LNAME,
        PROB_ID,
        PNAME,
        CPID,
        CP_NAME,
        CP_FSM,
        SCORE_REVEALING,
        CRID,
        CR_NAME,
        FULL_RES,
        CRENDS,
        CID,
        CNAME,
        CREATED_AT,
        PFINAL,
        CFINAL,
        CINIFINAL,
        INITIAL_STATUS,
        FINAL_STATUS,
        SCORE,
        INIT_REPORT,
        FINAL_REPORT
    };

    bool allow_access = (session->user_type == User::Type::ADMIN);
    bool select_one = false;
    bool selecting_problem_submissions = false;
    bool selecting_contest_submissions = false;

    bool selecting_finals = false;
    bool may_see_problem_final = (session->user_type == User::Type::ADMIN);

    auto append_column_names = [&] {
        // clang-format off
        append("[\n{\"columns\":["
                   "\"id\","
                   "\"type\","
                   "\"language\","
                   "\"user_id\","
                   "\"user_username\","
                   "\"user_first_name\","
                   "\"user_last_name\","
                   "\"problem_id\","
                   "\"problem_name\","
                   "\"contest_problem_id\","
                   "\"contest_problem_name\","
                   "\"contest_round_id\","
                   "\"contest_round_name\","
                   "\"contest_id\","
                   "\"contest_name\","
                   "\"created_at\","
                   "{\"name\":\"status\",\"fields\":[\"class\",\"text\"]},"
                   "\"score\","
                   "\"actions\"");
        // clang-format on

        if (select_one) {
            append(",\"initial_report\","
                   "\"final_report\","
                   "\"full_results\"");
        }

        append("]}");
    };

    auto set_empty_response = [&] {
        resp.content.clear();
        append_column_names();
        append("\n]");
    };

    // Process restrictions
    auto rows_limit = API_FIRST_QUERY_ROWS_LIMIT;
    {
        std::vector<std::function<void()>> after_checks;
        // At most one perms optional will be set
        std::optional<sim::contests::Permissions> contest_perms;
        std::optional<sim::problems::Permissions> problem_perms;

        bool id_condition_occurred = false;
        bool type_condition_occurred = false;
        bool round_or_problem_condition_occurred = false;
        bool user_condition_occurred = false;

        for (StringView next_arg = url_args.extract_next_arg(); not next_arg.empty();
             next_arg = url_args.extract_next_arg())
        {
            auto arg = decode_uri(next_arg);
            char cond_c = arg[0];
            StringView arg_id = StringView(arg).substr(1);

            // Submission type
            if (cond_c == 't') {
                if (type_condition_occurred) {
                    return api_error400("Submission type specified more than once");
                }

                type_condition_occurred = true;

                if (arg_id == "N") {
                    qwhere.append(" AND s.type=", EnumVal(OldSubmission::Type::NORMAL).to_int());
                } else if (arg_id == "F") {
                    selecting_finals = true;

                    after_checks.emplace_back([&] {
                        if (contest_perms.has_value() and
                            uint(
                                ~contest_perms.value() &
                                sim::contests::Permissions::SELECT_FINAL_SUBMISSIONS
                            ))
                        {
                            allow_access = false;
                        }

                        if (selecting_problem_submissions and not may_see_problem_final) {
                            allow_access = false;
                        }

                        if (not round_or_problem_condition_occurred) {
                            if (may_see_problem_final) {
                                qwhere.append(" AND (s.problem_final=1 OR "
                                              "s.contest_problem_final=1)");
                            } else {
                                qwhere.append(" AND s.contest_problem_final=1");
                            }
                        } else if (selecting_problem_submissions) {
                            qwhere.append(" AND s.problem_final=1");
                        } else if (selecting_contest_submissions) {
                            // TODO: double check that it works
                            qwhere.append(" AND s.contest_problem_final=1");
                        }
                    });
                } else if (arg_id == "I") {
                    qwhere.append(" AND s.type=", EnumVal(OldSubmission::Type::IGNORED).to_int());
                } else if (arg_id == "S") {
                    qwhere.append(
                        " AND s.type=", EnumVal(OldSubmission::Type::PROBLEM_SOLUTION).to_int()
                    );
                } else {
                    return api_error400(from_unsafe{concat("Invalid submission type: ", arg_id)});
                }

                // NOLINTNEXTLINE(bugprone-branch-clone)
            } else if (not is_digit(arg_id)) { // Next conditions require arg_id
                                               // to be a valid ID
                return api_error400();

            } else if (is_one_of(cond_c, '<', '>')) {
                if (id_condition_occurred) {
                    return api_error400("Submission ID condition specified more than once");
                }

                rows_limit = API_OTHER_QUERY_ROWS_LIMIT;
                id_condition_occurred = true;
                qwhere.append(" AND s.id", arg);

            } else if (cond_c == '=') {
                if (id_condition_occurred) {
                    return api_error400("Submission ID condition specified more than once");
                }

                id_condition_occurred = true;
                select_one = true;
                allow_access = true; // Permissions will be checked while fetching the data

                qfields.append(", s.initial_report, s.final_report");
                qwhere.append(" AND s.id", arg);

            } else if (cond_c == 'p') { // Problem's id
                if (round_or_problem_condition_occurred) {
                    return api_error400("Round or problem ID condition "
                                        "specified more than once");
                }

                selecting_problem_submissions = true;
                round_or_problem_condition_occurred = true;
                qwhere.append(" AND s.problem_id=", arg_id);

                problem_perms = sim::problems::get_permissions(
                    mysql,
                    arg_id,
                    (session.has_value() ? optional{session->user_id} : std::nullopt),
                    (session.has_value() ? optional{session->user_type} : std::nullopt)
                );
                if (not problem_perms) {
                    return set_empty_response();
                }

                if (uint(
                        problem_perms.value() &
                        sim::problems::Permissions::VIEW_SOLUTIONS_AND_SUBMISSIONS
                    ))
                {
                    allow_access = true;
                }

                if (uint(
                        problem_perms.value() &
                        sim::problems::Permissions::VIEW_SOLUTIONS_AND_SUBMISSIONS
                    ))
                {
                    may_see_problem_final = true;
                }

            } else if (is_one_of(cond_c, 'C', 'R', 'P')) { // Round's id
                if (round_or_problem_condition_occurred) {
                    return api_error400("Round or problem ID condition is "
                                        "specified more than once");
                }

                selecting_contest_submissions = true;
                round_or_problem_condition_occurred = true;

                if (cond_c == 'C') {
                    qwhere.append(" AND s.contest_id=", arg_id);
                } else if (cond_c == 'R') {
                    qwhere.append(" AND s.contest_round_id=", arg_id);
                } else if (cond_c == 'P') {
                    qwhere.append(" AND s.contest_problem_id=", arg_id);
                }

                if (not allow_access) {
                    StringView query;
                    switch (cond_c) {
                    case 'C':
                        query = "SELECT cu.mode, c.is_public "
                                "FROM contests c "
                                "LEFT JOIN contest_users cu ON cu.user_id=?"
                                " AND cu.contest_id=c.id "
                                "WHERE c.id=?";
                        break;

                    case 'R':
                        query = "SELECT cu.mode, c.is_public "
                                "FROM contest_rounds r "
                                "LEFT JOIN contests c ON c.id=r.contest_id "
                                "LEFT JOIN contest_users cu"
                                " ON cu.contest_id=r.contest_id"
                                " AND cu.user_id=? "
                                "WHERE r.id=?";
                        break;

                    case 'P':
                        query = "SELECT cu.mode, c.is_public "
                                "FROM contest_problems p "
                                "LEFT JOIN contests c ON c.id=p.contest_id "
                                "LEFT JOIN contest_users cu"
                                " ON cu.contest_id=p.contest_id"
                                " AND cu.user_id=? "
                                "WHERE p.id=?";
                        break;
                    default: assert(false);
                    }
                    auto old_mysql = old_mysql::ConnectionView{mysql};
                    auto stmt = old_mysql.prepare(query);
                    stmt.bind_and_execute(session->user_id, arg_id);

                    old_mysql::Optional<decltype(OldContestUser::mode)> cu_mode;
                    uint8_t is_public = false;
                    stmt.res_bind_all(cu_mode, is_public);
                    if (not stmt.next()) {
                        return set_empty_response();
                    }

                    contest_perms = sim::contests::get_permissions(
                        (session.has_value() ? std::optional{session->user_type} : std::nullopt),
                        is_public,
                        cu_mode
                    );
                    if (uint(
                            contest_perms.value() &
                            sim::contests::Permissions::VIEW_ALL_CONTEST_SUBMISSIONS
                        ))
                    {
                        allow_access = true;
                    }
                }

            } else if (cond_c == 'u') { // User's ID (submission's user_id)
                if (user_condition_occurred) {
                    return api_error400("User ID condition specified more than once");
                }

                user_condition_occurred = true;
                qwhere.append(" AND s.user_id=", arg_id);

                // Owner (almost) always has access to theirs submissions
                if (str2num<decltype(session->user_id)>(arg_id) == session->user_id) {
                    after_checks.emplace_back([&] {
                        if (allow_access) {
                            return;
                        }
                        // Need to check the problem permissions not to leak the
                        // information about which submission is the final one -
                        // it may be used during the contest with hidden results
                        // where the user is not allowed to access problem in
                        // Problems
                        if (selecting_problem_submissions) {
                            allow_access = (may_see_problem_final or not selecting_finals);
                        } else {
                            allow_access = (not selecting_finals);
                        }
                    });
                }

            } else {
                return api_error400();
            }
        }

        for (auto& check : after_checks) {
            check();
        }
    }

    if (not allow_access) {
        return set_empty_response();
    }

    // Execute query
    auto old_mysql = old_mysql::ConnectionView{mysql};
    auto res = old_mysql.query(qfields, qwhere, " ORDER BY s.id DESC LIMIT ", rows_limit);

    append_column_names();

    auto curr_date = utc_mysql_datetime();
    while (res.next()) {
        EnumVal<OldSubmission::Type> stype{
            WONT_THROW(str2num<OldSubmission::Type::UnderlyingType>(res[STYPE]).value())
        };
        SubmissionPermissions perms = submissions_get_permissions(
            (res.is_null(SUSER)
                 ? std::nullopt
                 : optional<
                       decltype(sim::submissions::OldSubmission::user_id)::value_type>{WONT_THROW(
                       str2num<decltype(sim::submissions::OldSubmission::user_id)::value_type>(
                           res[SUSER]
                       )
                           .value()
                   )}),
            stype,
            (res.is_null(CUMODE)
                 ? std::nullopt
                 : std::optional{decltype(OldContestUser::mode){WONT_THROW(
                       str2num<decltype(OldContestUser::mode)::ValType>(res[CUMODE]).value()
                   )}}),
            (res.is_null(POWNER_ID)
                 ? std::nullopt
                 : optional<decltype(OldProblem::owner_id)::value_type>{WONT_THROW(
                       str2num<decltype(OldProblem::owner_id)::value_type>(res[POWNER_ID]).value()
                   )})
        );

        if (perms == PERM::NONE) {
            return set_empty_response();
        }

        bool contest_submission = not res.is_null(CID);

        InfDatetime full_results;
        if (not contest_submission) { // The submission is not in a contest, so
            // full results are visible immediately
            full_results.set_neg_inf();
        } else {
            full_results.from_str(res[FULL_RES]);
        }

        bool show_full_results =
            (bool(uint(perms & PERM::VIEW_FINAL_REPORT)) or full_results <= curr_date);
        auto is_problem_final = WONT_THROW(
            str2num<decltype(OldSubmission::problem_final)::int_type>(res[PFINAL]).value()
        );
        auto is_contest_problem_final = WONT_THROW(
            str2num<decltype(OldSubmission::contest_problem_final)::int_type>(res[CFINAL]).value()
        );
        auto is_contest_problem_initial_final =
            WONT_THROW(str2num<decltype(OldSubmission::contest_problem_initial_final)::int_type>(
                           res[CINIFINAL]
            )
                           .value());

        // Submission id
        append(",\n[", res[SID], ',');

        optional<decltype(OldContestProblem::score_revealing)> score_revealing;
        if (not res.is_null(SCORE_REVEALING)) {
            score_revealing.emplace(WONT_THROW(
                str2num<decltype(OldContestProblem::score_revealing)::ValType>(res[SCORE_REVEALING])
                    .value()
            ));
        }

        // Type
        switch (stype) {
        case OldSubmission::Type::NORMAL: {
            enum SubmissionNormalSubtype {
                NORMAL,
                FINAL,
                PROBLEM_FINAL,
                INITIAL_FINAL
            } subtype_to_show = NORMAL; // Silence GCC warning

            auto problem_final_to_subtype = [&] {
                return (is_problem_final and may_see_problem_final ? PROBLEM_FINAL : NORMAL);
            };
            if (not contest_submission) {
                subtype_to_show = problem_final_to_subtype();
            } else {
                auto full_results_subtype = [&] {
                    if (is_contest_problem_final) {
                        return FINAL;
                    }
                    if (selecting_contest_submissions) {
                        return NORMAL;
                    }

                    return problem_final_to_subtype();
                };

                if (selecting_problem_submissions) {
                    subtype_to_show = problem_final_to_subtype();
                } else if (show_full_results) {
                    subtype_to_show = full_results_subtype();
                } else {
                    throw_assert(score_revealing.has_value()); // This is a contest submission
                    switch (score_revealing.value()) {
                    case OldContestProblem::ScoreRevealing::NONE:
                    case OldContestProblem::ScoreRevealing::ONLY_SCORE: {
                        subtype_to_show =
                            (is_contest_problem_initial_final ? INITIAL_FINAL : NORMAL);
                        break;
                    }
                    case OldContestProblem::ScoreRevealing::SCORE_AND_FULL_STATUS: {
                        subtype_to_show = full_results_subtype();
                        break;
                    }
                    }
                }
            }

            if (subtype_to_show != FINAL and subtype_to_show != PROBLEM_FINAL and selecting_finals)
            {
                if (not select_one) {
                    THROW(
                        "Cannot show final while selecting finals - this is "
                        "a bug in the query or the restricting access (URL: ",
                        request.target,
                        ')'
                    );
                }

                return set_empty_response();
            }

            switch (subtype_to_show) {
            case NORMAL: append("\"Normal\","); break;
            case FINAL: append("\"Final\","); break;
            case PROBLEM_FINAL: append("\"Problem final\","); break;
            case INITIAL_FINAL: append("\"Initial final\","); break;
            }
            break;
        }
        case OldSubmission::Type::IGNORED: append("\"Ignored\","); break;
        case OldSubmission::Type::PROBLEM_SOLUTION: append("\"Problem solution\","); break;
        }

        append(
            '"',
            to_string(OldSubmission::Language(WONT_THROW(
                str2num<decltype(OldSubmission::language)::ValType>(res[SLANGUAGE]).value()
            ))),
            "\","
        );

        // Onwer's id
        if (res.is_null(SUSER)) {
            append("null,");
        } else {
            append(res[SUSER], ',');
        }

        // Onwer's username
        if (res.is_null(SUSER_USERNAME)) {
            append("null,");
        } else {
            append("\"", res[SUSER_USERNAME], "\",");
        }

        // Onwer's first name
        if (res.is_null(SUSER_FNAME)) {
            append("null,");
        } else {
            append(json_stringify(res[SUSER_FNAME]), ',');
        }

        // Onwer's last name
        if (res.is_null(SUSER_LNAME)) {
            append("null,");
        } else {
            append(json_stringify(res[SUSER_LNAME]), ',');
        }

        // Problem (id, name)
        if (res.is_null(PNAME)) {
            append("null,null,");
        } else {
            append(res[PROB_ID], ',', json_stringify(res[PNAME]), ',');
        }

        // Contest problem (id, name)
        if (res.is_null(CP_NAME)) {
            append("null,null,");
        } else {
            append(res[CPID], ',', json_stringify(res[CP_NAME]), ',');
        }

        // Contest round (id, name)
        if (res.is_null(CR_NAME)) {
            append("null,null,");
        } else {
            append(res[CRID], ',', json_stringify(res[CR_NAME]), ',');
        }

        // Contest (id, name)
        if (res.is_null(CNAME)) {
            append("null,null,");
        } else {
            append(res[CID], ',', json_stringify(res[CNAME]), ',');
        }

        // Submit time
        append("\"", res[CREATED_AT], "\",");

        bool show_full_status = (show_full_results or [&] {
            if (score_revealing) {
                switch (score_revealing.value()) {
                case OldContestProblem::ScoreRevealing::NONE:
                case OldContestProblem::ScoreRevealing::ONLY_SCORE: return false;
                case OldContestProblem::ScoreRevealing::SCORE_AND_FULL_STATUS: return true;
                }
            }

            return false;
        }());

        bool show_score = (show_full_results or [&] {
            if (score_revealing) {
                switch (score_revealing.value()) {
                case OldContestProblem::ScoreRevealing::NONE: return false;
                case OldContestProblem::ScoreRevealing::ONLY_SCORE:
                case OldContestProblem::ScoreRevealing::SCORE_AND_FULL_STATUS: return true;
                }
            }

            return false;
        }());

        // Status: (CSS class, text)
        append_submission_status(
            decltype(OldSubmission::initial_status)(WONT_THROW(
                str2num<decltype(OldSubmission::initial_status)::ValType>(res[INITIAL_STATUS])
                    .value()
            )),
            decltype(OldSubmission::full_status)(WONT_THROW(
                str2num<decltype(OldSubmission::full_status)::ValType>(res[FINAL_STATUS]).value()
            )),
            show_full_status
        );

        // Score
        if (not res.is_null(SCORE) and show_score) {
            append(',', res[SCORE], ',');
        } else {
            append(",null,");
        }

        // Actions
        append('"');
        if (uint(perms & PERM::VIEW)) {
            append('v');
        }
        if (uint(perms & PERM::VIEW_SOURCE)) {
            append('s');
        }
        if (uint(perms & PERM::VIEW_RELATED_JOBS)) {
            append('j');
        }
        if (uint(perms & PERM::VIEW) and
            (res.is_null(CRENDS) or curr_date < InfDatetime(res[CRENDS])))
        {
            //  ^ submission to a problem;    ^ Round has not ended
            // TODO: implement it in some way in the UI
            append('r'); // Resubmit solution
        }
        if (uint(perms & PERM::CHANGE_TYPE)) {
            append('C');
        }
        if (uint(perms & PERM::REJUDGE)) {
            append('R');
        }
        if (uint(perms & PERM::DELETE)) {
            append('D');
        }

        append("\"");

        // Append
        if (select_one) {
            // Reports (and round full results time if full report isn't shown)
            append(',', json_stringify(res[INIT_REPORT]));
            if (show_full_results) {
                append(',', json_stringify(res[FINAL_REPORT]));
            } else {
                append(",null,\"", full_results.to_api_str(), '"');
            }
        }

        append(']');
    }

    append("\n]");
}

void Sim::api_submission() {
    STACK_UNWINDING_MARK;

    if (not session.has_value()) {
        return api_error403();
    }

    StringView next_arg = url_args.extract_next_arg();
    if (next_arg == "add") {
        return api_submission_add();
    }
    if (not is_digit(next_arg)) {
        return api_error400();
    }

    submissions_sid = next_arg;

    old_mysql::Optional<decltype(OldSubmission::user_id)::value_type> suser_id;
    old_mysql::Optional<decltype(OldProblem::owner_id)::value_type> p_owner_id;
    EnumVal<OldSubmission::Type> stype{};
    EnumVal<OldSubmission::Language> slang{};
    old_mysql::Optional<decltype(OldContestUser::mode)> cu_mode;
    auto old_mysql = old_mysql::ConnectionView{mysql};
    auto stmt = old_mysql.prepare("SELECT s.file_id, s.user_id, s.type, s.language,"
                                  " cu.mode, p.owner_id "
                                  "FROM submissions s "
                                  "STRAIGHT_JOIN problems p ON p.id=s.problem_id "
                                  "LEFT JOIN contest_users cu"
                                  " ON cu.contest_id=s.contest_id AND cu.user_id=? "
                                  "WHERE s.id=?");
    stmt.bind_and_execute(session->user_id, submissions_sid);
    stmt.res_bind_all(submissions_file_id, suser_id, stype, slang, cu_mode, p_owner_id);
    if (not stmt.next()) {
        return api_error404();
    }

    submissions_slang = slang;
    submissions_perms = submissions_get_permissions(suser_id, stype, cu_mode, p_owner_id);

    // Subpages
    next_arg = url_args.extract_next_arg();
    if (next_arg == "source") {
        return api_submission_source();
    }
    if (next_arg == "download") {
        return api_submission_download();
    }

    if (request.method != http::Request::POST) {
        return api_error400();
    }

    // Subpages causing an action
    if (next_arg == "rejudge") {
        return api_submission_rejudge();
    }
    if (next_arg == "chtype") {
        return api_submission_change_type();
    }
    if (next_arg == "delete") {
        return api_submission_delete();
    }

    return api_error404();
}

void Sim::api_submission_add() {
    STACK_UNWINDING_MARK;

    if (not session.has_value()) {
        return api_error403();
    }

    StringView next_arg = url_args.extract_next_arg();
    // Load problem id and contest problem id (if specified)
    StringView problem_id;
    std::optional<InplaceBuff<32>> contest_problem_id;
    for (; !next_arg.empty(); next_arg = url_args.extract_next_arg()) {
        if (next_arg[0] == 'p' and is_digit(next_arg.substr(1)) and problem_id.empty()) {
            problem_id = next_arg.substr(1);
        } else if (has_prefix(next_arg, "cp") and is_digit(next_arg.substr(2)) and
                   not contest_problem_id.has_value())
        {
            contest_problem_id = next_arg.substr(2);
        } else {
            return api_error400();
        }
    }

    if (problem_id.empty()) {
        return api_error400("problem have to be specified");
    }

    bool may_submit_ignored = false;
    old_mysql::Optional<InplaceBuff<32>> contest_id;
    old_mysql::Optional<InplaceBuff<32>> contest_round_id;
    if (not contest_problem_id.has_value()) { // Problem submission
        // Check permissions to the problem
        auto problem_perms_opt = sim::problems::get_permissions(
            mysql,
            problem_id,
            (session.has_value() ? optional{session->user_id} : std::nullopt),
            (session.has_value() ? optional{session->user_type} : std::nullopt)
        );
        if (not problem_perms_opt) {
            return api_error404();
        }

        auto problem_perms = problem_perms_opt.value();
        if (uint(~problem_perms & sim::problems::Permissions::SUBMIT)) {
            return api_error403();
        }

        may_submit_ignored = uint(problem_perms & sim::problems::Permissions::SUBMIT_IGNORED);

    } else { // Contest submission
        // Check permissions to the contest
        auto old_mysql = old_mysql::ConnectionView{mysql};
        auto stmt = old_mysql.prepare("SELECT c.id, c.is_public, cr.id, cr.begins,"
                                      " cr.ends, cu.mode "
                                      "FROM contest_problems cp "
                                      "STRAIGHT_JOIN contest_rounds cr"
                                      " ON cr.id=cp.contest_round_id "
                                      "STRAIGHT_JOIN contests c"
                                      " ON c.id=cp.contest_id "
                                      "LEFT JOIN contest_users cu"
                                      " ON cu.contest_id=c.id AND cu.user_id=? "
                                      "WHERE cp.id=? AND cp.problem_id=?");
        stmt.bind_and_execute(session->user_id, contest_problem_id.value(), problem_id);

        decltype(sim::contests::OldContest::is_public) is_public;
        InplaceBuff<20> cr_begins_str;
        InplaceBuff<20> cr_ends_str;
        old_mysql::Optional<decltype(OldContestUser::mode)> umode;
        stmt.res_bind_all(
            contest_id, is_public, contest_round_id, cr_begins_str, cr_ends_str, umode
        );
        if (not stmt.next()) {
            return api_error404();
        }

        auto contest_perms = sim::contests::get_permissions(
            (session.has_value() ? std::optional{session->user_type} : std::nullopt),
            is_public,
            umode
        );
        if (uint(~contest_perms & sim::contests::Permissions::PARTICIPATE)) {
            return api_error403(); // Could not participate
        }

        auto curr_date = utc_mysql_datetime();
        if (uint(~contest_perms & sim::contests::Permissions::ADMIN) and
            (curr_date < InfDatetime(cr_begins_str) or InfDatetime(cr_ends_str) <= curr_date))
        {
            return api_error403(); // Round has not begun jet or already ended
        }

        may_submit_ignored = uint(contest_perms & sim::contests::Permissions::ADMIN);
    }

    // Validate fields
    OldSubmission::Language slang = OldSubmission::Language::C11; // Silence GCC warning
    auto slang_str = request.form_fields.get("language");
    if (slang_str == "c11") {
        slang = OldSubmission::Language::C11;
    } else if (slang_str == "cpp11") {
        slang = OldSubmission::Language::CPP11;
    } else if (slang_str == "cpp14") {
        slang = OldSubmission::Language::CPP14;
    } else if (slang_str == "cpp17") {
        slang = OldSubmission::Language::CPP17;
    } else if (slang_str == "pascal") {
        slang = OldSubmission::Language::PASCAL;
    } else if (slang_str == "python") {
        slang = OldSubmission::Language::PYTHON;
    } else if (slang_str == "rust") {
        slang = OldSubmission::Language::RUST;
    } else {
        add_notification("error", "Invalid language");
    }

    auto solution_tmp_path_opt = request.form_fields.file_path("solution");
    if (!solution_tmp_path_opt) {
        add_notification("error", "Missing solution file");
        return api_error400(notifications);
    }

    StringView code = request.form_fields.get("code").value_or("");
    bool ignored_submission = (may_submit_ignored and request.form_fields.contains("ignored"));

    if ((code.empty() ^ request.form_fields.get("solution").value_or("").empty()) == 0) {
        add_notification("error", "You have to either choose a file or paste the code");
        return api_error400(notifications);
    }

    // Check the solution size
    if ((code.empty() ? get_file_size(*solution_tmp_path_opt) : code.size()) >
        OldSubmission::solution_max_size)
    {
        add_notification(
            "error",
            "Solution is too big (maximum allowed size: ",
            OldSubmission::solution_max_size,
            " bytes = ",
            humanize_file_size(OldSubmission::solution_max_size),
            ')'
        );
        return api_error400(notifications);
    }

    auto transaction = mysql.start_repeatable_read_transaction();

    auto old_mysql = old_mysql::ConnectionView{mysql};
    old_mysql.prepare("INSERT INTO internal_files (created_at) VALUES(?)")
        .bind_and_execute(utc_mysql_datetime());
    auto file_id = old_mysql.insert_id();
    CallInDtor file_remover([file_id] { (void)unlink(sim::internal_files::old_path_of(file_id)); });

    // Save source file
    if (not code.empty()) {
        put_file_contents(sim::internal_files::old_path_of(file_id), code);
    } else if (move(*solution_tmp_path_opt, sim::internal_files::old_path_of(file_id))) {
        THROW("move()", errmsg());
    }

    // Insert submission
    auto stmt = old_mysql.prepare("INSERT submissions (file_id, user_id, problem_id,"
                                  " contest_problem_id, contest_round_id,"
                                  " contest_id, type, language, initial_status,"
                                  " full_status, created_at, last_judgment_began_at,"
                                  " initial_report, final_report) "
                                  "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, NULL, '',"
                                  " '')");
    stmt.bind_and_execute(
        file_id,
        session->user_id,
        problem_id,
        contest_problem_id,
        contest_round_id,
        contest_id,
        EnumVal(ignored_submission ? OldSubmission::Type::IGNORED : OldSubmission::Type::NORMAL),
        EnumVal(slang),
        EnumVal(OldSubmission::Status::PENDING),
        EnumVal(OldSubmission::Status::PENDING),
        utc_mysql_datetime()
    );

    // Create a job to judge the submission
    auto submission_id = stmt.insert_id();
    old_mysql
        .prepare("INSERT jobs (creator, status, priority, type, created_at, aux_id) "
                 "VALUES(?, ?, ?, ?, ?, ?)")
        .bind_and_execute(
            session->user_id,
            EnumVal(OldJob::Status::PENDING),
            default_priority(OldJob::Type::JUDGE_SUBMISSION),
            EnumVal(OldJob::Type::JUDGE_SUBMISSION),
            utc_mysql_datetime(),
            submission_id
        );

    transaction.commit();
    file_remover.cancel();

    sim::job_server::notify_job_server();
    append(submission_id);
}

void Sim::api_submission_source() {
    STACK_UNWINDING_MARK;

    if (uint(~submissions_perms & SubmissionPermissions::VIEW_SOURCE)) {
        return api_error403();
    }

    append(cpp_syntax_highlighter(
        from_unsafe{get_file_contents(sim::internal_files::old_path_of(submissions_file_id))}
    ));
}

void Sim::api_submission_download() {
    STACK_UNWINDING_MARK;

    if (uint(~submissions_perms & SubmissionPermissions::VIEW_SOURCE)) {
        return api_error403();
    }

    resp.headers["Content-type"] = to_mime(submissions_slang);
    resp.headers["Content-Disposition"] =
        concat_tostr("attachment; filename=", submissions_sid, to_extension(submissions_slang));

    resp.content = sim::internal_files::old_path_of(submissions_file_id);
    resp.content_type = http::Response::FILE;
}

void Sim::api_submission_rejudge() {
    STACK_UNWINDING_MARK;

    if (uint(~submissions_perms & SubmissionPermissions::REJUDGE)) {
        return api_error403();
    }

    InplaceBuff<32> problem_id;
    auto old_mysql = old_mysql::ConnectionView{mysql};
    auto stmt = old_mysql.prepare("SELECT problem_id FROM submissions WHERE id=?");
    stmt.bind_and_execute(submissions_sid);
    stmt.res_bind_all(problem_id);
    throw_assert(stmt.next());

    stmt = old_mysql.prepare("INSERT jobs (creator, status, priority, type, created_at, aux_id) "
                             "VALUES(?, ?, ?, ?, ?, ?)");
    stmt.bind_and_execute(
        session->user_id,
        EnumVal(OldJob::Status::PENDING),
        default_priority(OldJob::Type::REJUDGE_SUBMISSION),
        EnumVal(OldJob::Type::REJUDGE_SUBMISSION),
        utc_mysql_datetime(),
        submissions_sid
    );

    sim::job_server::notify_job_server();
}

void Sim::api_submission_change_type() {
    STACK_UNWINDING_MARK;

    if (uint(~submissions_perms & SubmissionPermissions::CHANGE_TYPE)) {
        return api_error403();
    }

    auto type_s = request.form_fields.get("type");

    std::optional<sim::submissions::Submission::Type> new_type;
    if (type_s == "I") {
        new_type = sim::submissions::Submission::Type::IGNORED;
    } else if (type_s == "N") {
        new_type = sim::submissions::Submission::Type::NORMAL;
    } else {
        return api_error400("Invalid type, it must be one of those: I or N");
    }

    auto transaction = mysql.start_repeatable_read_transaction();

    auto stmt = mysql.execute(
        sim::sql::Select("s.user_id, s.problem_id, s.contest_problem_id, "
                         "cp.score_revealing, s.initial_status, s.full_status, s.score")
            .from("submissions s")
            .left_join("contest_problems cp")
            .on("cp.id=s.contest_problem_id")
            .where("s.id=?", submissions_sid)
    );
    decltype(sim::submissions::Submission::user_id) user_id;
    decltype(sim::submissions::Submission::problem_id) problem_id;
    decltype(sim::submissions::Submission::contest_problem_id) contest_problem_id;
    std::optional<decltype(sim::contest_problems::ContestProblem::score_revealing)>
        contest_problem_score_revealing;
    decltype(sim::submissions::Submission::initial_status) initial_status;
    decltype(sim::submissions::Submission::full_status) full_status;
    decltype(sim::submissions::Submission::score) score;
    stmt.res_bind(
        user_id,
        problem_id,
        contest_problem_id,
        contest_problem_score_revealing,
        initial_status,
        full_status,
        score
    );
    if (!stmt.next()) {
        // May happen in the old API if the submission is deleted between transactions
        return api_error404();
    }

    mysql.execute(
        sim::sql::Update("submissions")
            .set(
                "type=?, final_candidate=?, initial_final_candidate=?",
                new_type,
                sim::submissions::is_final_candidate(*new_type, full_status, score),
                sim::submissions::is_initial_final_candidate(
                    *new_type, initial_status, full_status, score, contest_problem_score_revealing
                )
            )
            .where("id=?", submissions_sid)
    );

    sim::submissions::update_final(mysql, user_id, problem_id, contest_problem_id);
    transaction.commit();
}

void Sim::api_submission_delete() {
    STACK_UNWINDING_MARK;

    if (uint(~submissions_perms & SubmissionPermissions::DELETE)) {
        return api_error403();
    }

    auto transaction = mysql.start_repeatable_read_transaction();

    auto old_mysql = old_mysql::ConnectionView{mysql};
    auto stmt = old_mysql.prepare("SELECT user_id, problem_id, contest_problem_id "
                                  "FROM submissions WHERE id=?");
    stmt.bind_and_execute(submissions_sid);
    old_mysql::Optional<uint64_t> user_id;
    old_mysql::Optional<uint64_t> contest_problem_id;
    uint64_t problem_id = 0;
    stmt.res_bind_all(user_id, problem_id, contest_problem_id);
    throw_assert(stmt.next());

    old_mysql
        .prepare("INSERT INTO jobs(creator, type, priority, status, created_at, aux_id)"
                 "SELECT NULL, ?, ?, ?, ?, file_id"
                 " FROM submissions WHERE id=?")
        .bind_and_execute(
            EnumVal(OldJob::Type::DELETE_INTERNAL_FILE),
            default_priority(OldJob::Type::DELETE_INTERNAL_FILE),
            EnumVal(OldJob::Status::PENDING),
            utc_mysql_datetime(),
            submissions_sid
        );

    old_mysql.prepare("DELETE FROM submissions WHERE id=?").bind_and_execute(submissions_sid);

    sim::submissions::update_final(mysql, user_id, problem_id, contest_problem_id);

    transaction.commit();
    sim::job_server::notify_job_server();
}

} // namespace web_server::old

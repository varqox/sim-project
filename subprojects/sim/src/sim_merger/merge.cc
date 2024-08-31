#include "deduplicate_problems.hh"
#include "merge.hh"
#include "mergers/add_problem_jobs_merger.hh"
#include "mergers/change_problem_statement_jobs_merger.hh"
#include "mergers/contest_entry_tokens_merger.hh"
#include "mergers/contest_files_merger.hh"
#include "mergers/contest_problems_merger.hh"
#include "mergers/contest_rounds_merger.hh"
#include "mergers/contest_users_merger.hh"
#include "mergers/contests_merger.hh"
#include "mergers/internal_files_merger.hh"
#include "mergers/jobs_merger.hh"
#include "mergers/merge_problems_jobs_merger.hh"
#include "mergers/problem_tags_merger.hh"
#include "mergers/problems_merger.hh"
#include "mergers/reupload_problem_jobs_merger.hh"
#include "mergers/sessions_merger.hh"
#include "mergers/submissions_merger.hh"
#include "mergers/users_merger.hh"
#include "sim_instance.hh"

#include <sim/db/tables.hh>
#include <simlib/logger.hh>
#include <simlib/macros/stack_unwinding.hh>

void merge(const SimInstance& main_sim, const SimInstance& other_sim) {
    STACK_UNWINDING_MARK;

    static_assert(
        sim::db::TABLES_NUM == 18, "Number of tables changed, you need to update the code below"
    );

    stdlog("\033[1mCollecting internal_files ids\033[m");
    auto internal_files_merger = mergers::InternalFilesMerger{main_sim, other_sim};
    stdlog("\033[1mMerging internal_files\033[m");
    internal_files_merger.merge_and_save();

    stdlog("\033[1mCollecting user ids\033[m");
    auto users_merger = mergers::UsersMerger{main_sim, other_sim};
    stdlog("\033[1mSaving merged users\033[m");
    users_merger.merge_and_save();

    stdlog("\033[1mMerging sessions\033[m");
    auto sessions_merger = mergers::SessionsMerger{main_sim, other_sim, users_merger};
    sessions_merger.merge_and_save();

    stdlog("\033[1mCollecting problems ids\033[m");
    auto problems_merger =
        mergers::ProblemsMerger{main_sim, other_sim, internal_files_merger, users_merger};
    stdlog("\033[1mMerging problems\033[m");
    problems_merger.merge_and_save();

    stdlog("\033[1mMerging problem_tags\033[m");
    auto problem_tags_merger = mergers::ProblemTagsMerger{main_sim, other_sim, problems_merger};
    problem_tags_merger.merge_and_save();

    stdlog("\033[1mCollecting contests ids\033[m");
    auto contests_merger = mergers::ContestsMerger{main_sim, other_sim};
    stdlog("\033[1mMerging contests\033[m");
    contests_merger.merge_and_save();

    stdlog("\033[1mCollecting contest_rounds ids\033[m");
    auto contest_rounds_merger = mergers::ContestRoundsMerger{main_sim, other_sim, contests_merger};
    stdlog("\033[1mMerging contest_rounds\033[m");
    contest_rounds_merger.merge_and_save();

    stdlog("\033[1mCollecting contest_problems ids\033[m");
    auto contest_problems_merger = mergers::ContestProblemsMerger{
        main_sim, other_sim, contests_merger, contest_rounds_merger, problems_merger
    };
    stdlog("\033[1mMerging contest_problems\033[m");
    contest_problems_merger.merge_and_save();

    stdlog("\033[1mMerging contest_users\033[m");
    auto contest_users_merger =
        mergers::ContestUsersMerger{main_sim, other_sim, contests_merger, users_merger};
    contest_users_merger.merge_and_save();

    stdlog("\033[1mMerging contest_files\033[m");
    auto contest_files_merger = mergers::ContestFilesMerger{
        main_sim, other_sim, contests_merger, internal_files_merger, users_merger
    };
    contest_files_merger.merge_and_save();

    stdlog("\033[1mMerging contest_entry_tokens\033[m");
    auto contest_entry_tokens_merger =
        mergers::ContestEntryTokensMerger{main_sim, other_sim, contests_merger};
    contest_entry_tokens_merger.merge_and_save();

    stdlog("\033[1mCollecting submissions ids\033[m");
    auto submissions_merger = mergers::SubmissionsMerger{
        main_sim,
        other_sim,
        internal_files_merger,
        users_merger,
        problems_merger,
        contest_problems_merger,
        contest_rounds_merger,
        contests_merger,
    };
    stdlog("\033[1mMerging submissions\033[m");
    submissions_merger.merge_and_save();

    stdlog("\033[1mCollecting jobs ids\033[m");
    auto jobs_merger = mergers::JobsMerger{
        main_sim,
        other_sim,
        internal_files_merger,
        users_merger,
        problems_merger,
        contests_merger,
        contest_rounds_merger,
        contest_problems_merger,
        submissions_merger,
    };
    stdlog("\033[1mMerging jobs\033[m");
    jobs_merger.merge_and_save();

    stdlog("\033[1mMerging add_problem_jobs\033[m");
    mergers::AddProblemJobsMerger{main_sim, other_sim, jobs_merger, internal_files_merger}
        .merge_and_save();

    stdlog("\033[1mMerging reupload_problem_jobs\033[m");
    mergers::ReuploadProblemJobsMerger{main_sim, other_sim, jobs_merger, internal_files_merger}
        .merge_and_save();

    stdlog("\033[1mMerging merge_problems_jobs\033[m");
    mergers::MergeProblemsJobsMerger{main_sim, other_sim, jobs_merger}.merge_and_save();

    stdlog("\033[1mMerging change_problem_statement_jobs\033[m");
    mergers::ChangeProblemStatementJobsMerger{
        main_sim, other_sim, jobs_merger, internal_files_merger
    }
        .merge_and_save();

    deduplicate_problems(main_sim);
}

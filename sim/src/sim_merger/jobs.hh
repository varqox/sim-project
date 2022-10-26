#pragma once

#include "sim/jobs/utils.hh"
#include "src/sim_merger/internal_files.hh"
#include "src/sim_merger/submissions.hh"
#include "src/sim_merger/users.hh"

namespace sim_merger {

class JobsMerger : public Merger<sim::jobs::Job> {
    const InternalFilesMerger& internal_files_;
    const UsersMerger& users_;
    const SubmissionsMerger& submissions_;
    const ProblemsMerger& problems_;
    const ContestsMerger& contests_;
    const ContestRoundsMerger& contest_rounds_;
    const ContestProblemsMerger& contest_problems_;

    void load(RecordSet& record_set) override {
        STACK_UNWINDING_MARK;
        using sim::jobs::Job;

        Job job;
        mysql::Optional<decltype(job.file_id)::value_type> m_file_id;
        mysql::Optional<decltype(job.tmp_file_id)::value_type> m_tmp_file_id;
        mysql::Optional<decltype(job.creator)::value_type> m_creator;
        mysql::Optional<decltype(job.aux_id)::value_type> m_aux_id;
        auto stmt = conn.prepare("SELECT id, file_id, tmp_file_id, creator, type,"
                                 " priority, status, added, aux_id, info, data "
                                 "FROM ",
                record_set.sql_table_name);
        stmt.bind_and_execute();
        stmt.res_bind_all(job.id, m_file_id, m_tmp_file_id, m_creator, job.type, job.priority,
                job.status, job.added, m_aux_id, job.info, job.data);
        while (stmt.next()) {
            job.file_id = m_file_id.to_opt();
            job.tmp_file_id = m_tmp_file_id.to_opt();
            job.creator = m_creator.to_opt();
            job.aux_id = m_aux_id.to_opt();

            if (job.file_id) {
                job.file_id = internal_files_.new_id(job.file_id.value(), record_set.kind);
            }
            if (job.tmp_file_id) {
                job.tmp_file_id = internal_files_.new_id(job.tmp_file_id.value(), record_set.kind);
            }
            if (job.creator) {
                job.creator = users_.new_id(job.creator.value(), record_set.kind);
            }

            // Process type-specific ids
            switch (job.type) {
            case Job::Type::DELETE_FILE:
                // Id is already processed
                break;

            case Job::Type::JUDGE_SUBMISSION:
            case Job::Type::REJUDGE_SUBMISSION:
                job.aux_id = submissions_.new_id(job.aux_id.value(), record_set.kind);
                break;

            case Job::Type::DELETE_PROBLEM:
            case Job::Type::REUPLOAD_PROBLEM:
            case Job::Type::REUPLOAD_PROBLEM__JUDGE_MODEL_SOLUTION:
            case Job::Type::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
            case Job::Type::CHANGE_PROBLEM_STATEMENT:
                job.aux_id = problems_.new_id(job.aux_id.value(), record_set.kind);
                break;

            case Job::Type::MERGE_PROBLEMS: {
                job.aux_id = problems_.new_id(job.aux_id.value(), record_set.kind);
                auto info = sim::jobs::MergeProblemsInfo(job.info);
                info.target_problem_id = problems_.new_id(info.target_problem_id, record_set.kind);
                job.info = info.dump();
                break;
            }

            case Job::Type::MERGE_USERS: {
                job.aux_id = users_.new_id(job.aux_id.value(), record_set.kind);
                auto info = sim::jobs::MergeUsersInfo(job.info);
                info.target_user_id = users_.new_id(info.target_user_id, record_set.kind);
                job.info = info.dump();
                break;
            }

            case Job::Type::DELETE_USER:
                job.aux_id = users_.new_id(job.aux_id.value(), record_set.kind);
                break;

            case Job::Type::DELETE_CONTEST:
                job.aux_id = contests_.new_id(job.aux_id.value(), record_set.kind);
                break;

            case Job::Type::DELETE_CONTEST_ROUND:
                job.aux_id = contest_rounds_.new_id(job.aux_id.value(), record_set.kind);
                break;

            case Job::Type::DELETE_CONTEST_PROBLEM:
            case Job::Type::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
                job.aux_id = contest_problems_.new_id(job.aux_id.value(), record_set.kind);
                break;

            case Job::Type::ADD_PROBLEM:
            case Job::Type::ADD_PROBLEM__JUDGE_MODEL_SOLUTION:
                if (job.aux_id) {
                    job.aux_id = problems_.new_id(job.aux_id.value(), record_set.kind);
                }
                break;

            case Job::Type::EDIT_PROBLEM: THROW("TODO");
            }

            record_set.add_record(
                    job, str_to_time_point(intentional_unsafe_cstring_view(job.added.to_string())));
        }
    }

    void merge() override {
        STACK_UNWINDING_MARK;
        Merger::merge([&](const sim::jobs::Job& /*unused*/) { return nullptr; });
    }

public:
    void save_merged() override {
        STACK_UNWINDING_MARK;
        auto transaction = conn.start_transaction();
        conn.update("TRUNCATE ", sql_table_name());
        auto stmt = conn.prepare("INSERT INTO ", sql_table_name(),
                "(id, file_id, tmp_file_id, creator, type, priority,"
                " status, added, aux_id, info, data) "
                "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

        ProgressBar progress_bar("Jobs saved:", new_table_.size(), 128);
        for (const NewRecord& new_record : new_table_) {
            Defer progressor = [&] { progress_bar.iter(); };
            const auto& x = new_record.data;
            stmt.bind_and_execute(x.id, x.file_id, x.tmp_file_id, x.creator, x.type, x.priority,
                    x.status, x.added, x.aux_id, x.info, x.data);
        }

        conn.update("ALTER TABLE ", sql_table_name(), " AUTO_INCREMENT=", last_new_id_ + 1);
        transaction.commit();
    }

    JobsMerger(const PrimaryKeysFromMainAndOtherJobs& ids_from_both_jobs,
            const InternalFilesMerger& internal_files, const UsersMerger& users,
            const SubmissionsMerger& submissions, const ProblemsMerger& problems,
            const ContestsMerger& contests, const ContestRoundsMerger& contest_rounds,
            const ContestProblemsMerger& contest_problems)
    : Merger("jobs", ids_from_both_jobs.main.jobs, ids_from_both_jobs.other.jobs)
    , internal_files_(internal_files)
    , users_(users)
    , submissions_(submissions)
    , problems_(problems)
    , contests_(contests)
    , contest_rounds_(contest_rounds)
    , contest_problems_(contest_problems) {
        STACK_UNWINDING_MARK;
        initialize();
    }
};

} // namespace sim_merger

#pragma once

#include "internal_files.hh"
#include "submissions.hh"
#include "users.hh"

#include <sim/jobs/utils.hh>

namespace sim_merger {

class JobsMerger : public Merger<sim::jobs::OldJob> {
    const InternalFilesMerger& internal_files_;
    const UsersMerger& users_;
    const SubmissionsMerger& submissions_;
    const ProblemsMerger& problems_;
    const ContestsMerger& contests_;
    const ContestRoundsMerger& contest_rounds_;
    const ContestProblemsMerger& contest_problems_;

    void load(RecordSet& record_set) override {
        STACK_UNWINDING_MARK;
        using sim::jobs::OldJob;

        OldJob job;
        old_mysql::Optional<decltype(job.file_id)::value_type> m_file_id;
        old_mysql::Optional<decltype(job.creator)::value_type> m_creator;
        old_mysql::Optional<decltype(job.aux_id)::value_type> m_aux_id;
        old_mysql::Optional<decltype(job.aux_id)::value_type> m_aux_id_2;
        auto old_mysql = old_mysql::ConnectionView{*mysql};
        auto stmt = old_mysql.prepare(
            "SELECT id, file_id, creator, type,"
            " priority, status, created_at, aux_id, aux_id_2, info, data "
            "FROM ",
            record_set.sql_table_name
        );
        stmt.bind_and_execute();
        stmt.res_bind_all(
            job.id,
            m_file_id,
            m_creator,
            job.type,
            job.priority,
            job.status,
            job.created_at,
            m_aux_id,
            m_aux_id_2,
            job.info,
            job.data
        );
        while (stmt.next()) {
            job.file_id = m_file_id.to_opt();
            job.creator = m_creator.to_opt();
            job.aux_id = m_aux_id.to_opt();
            job.aux_id_2 = m_aux_id_2.to_opt();

            if (job.file_id) {
                job.file_id = internal_files_.new_id(job.file_id.value(), record_set.kind);
            }
            if (job.creator) {
                job.creator = users_.new_id(job.creator.value(), record_set.kind);
            }

            // Process type-specific ids
            switch (job.type) {
            case OldJob::Type::DELETE_FILE:
                // Id is already processed
                break;

            case OldJob::Type::JUDGE_SUBMISSION:
            case OldJob::Type::REJUDGE_SUBMISSION:
                job.aux_id = submissions_.new_id(job.aux_id.value(), record_set.kind);
                break;

            case OldJob::Type::DELETE_PROBLEM:
            case OldJob::Type::REUPLOAD_PROBLEM:
            case OldJob::Type::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
            case OldJob::Type::CHANGE_PROBLEM_STATEMENT:
                job.aux_id = problems_.new_id(job.aux_id.value(), record_set.kind);
                break;

            case OldJob::Type::MERGE_PROBLEMS: {
                job.aux_id = problems_.new_id(job.aux_id.value(), record_set.kind);
                job.aux_id_2 = problems_.new_id(job.aux_id_2.value(), record_set.kind);
                break;
            }

            case OldJob::Type::MERGE_USERS: {
                job.aux_id = users_.new_id(job.aux_id.value(), record_set.kind);
                job.aux_id_2 = users_.new_id(job.aux_id_2.value(), record_set.kind);
                break;
            }

            case OldJob::Type::DELETE_USER:
                job.aux_id = users_.new_id(job.aux_id.value(), record_set.kind);
                break;

            case OldJob::Type::DELETE_CONTEST:
                job.aux_id = contests_.new_id(job.aux_id.value(), record_set.kind);
                break;

            case OldJob::Type::DELETE_CONTEST_ROUND:
                job.aux_id = contest_rounds_.new_id(job.aux_id.value(), record_set.kind);
                break;

            case OldJob::Type::DELETE_CONTEST_PROBLEM:
            case OldJob::Type::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
                job.aux_id = contest_problems_.new_id(job.aux_id.value(), record_set.kind);
                break;

            case OldJob::Type::ADD_PROBLEM:
                if (job.aux_id) {
                    job.aux_id = problems_.new_id(job.aux_id.value(), record_set.kind);
                }
                break;

            case OldJob::Type::EDIT_PROBLEM: THROW("TODO");
            }

            record_set.add_record(job, str_to_time_point(from_unsafe{job.created_at.to_string()}));
        }
    }

    void merge() override {
        STACK_UNWINDING_MARK;
        Merger::merge([&](const sim::jobs::OldJob& /*unused*/) { return nullptr; });
    }

public:
    void save_merged() override {
        STACK_UNWINDING_MARK;
        auto transaction = mysql->start_repeatable_read_transaction();
        auto old_mysql = old_mysql::ConnectionView{*mysql};
        old_mysql.update("TRUNCATE ", sql_table_name());
        auto stmt = old_mysql.prepare(
            "INSERT INTO ",
            sql_table_name(),
            "(id, file_id, creator, type, priority,"
            " status, created_at, aux_id, info, data) "
            "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
        );

        ProgressBar progress_bar("Jobs saved:", new_table_.size(), 128);
        for (const NewRecord& new_record : new_table_) {
            Defer progressor = [&] { progress_bar.iter(); };
            const auto& x = new_record.data;
            stmt.bind_and_execute(
                x.id,
                x.file_id,
                x.creator,
                x.type,
                x.priority,
                x.status,
                x.created_at,
                x.aux_id,
                x.info,
                x.data
            );
        }

        old_mysql.update("ALTER TABLE ", sql_table_name(), " AUTO_INCREMENT=", last_new_id_ + 1);
        transaction.commit();
    }

    JobsMerger(
        const PrimaryKeysFromMainAndOtherJobs& ids_from_both_jobs,
        const InternalFilesMerger& internal_files,
        const UsersMerger& users,
        const SubmissionsMerger& submissions,
        const ProblemsMerger& problems,
        const ContestsMerger& contests,
        const ContestRoundsMerger& contest_rounds,
        const ContestProblemsMerger& contest_problems
    )
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

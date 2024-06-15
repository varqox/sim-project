#pragma once

#include <sim/internal_files/internal_file.hh>
#include <sim/jobs/job.hh>
#include <sim/sql/fields/varbinary.hh>

namespace sim::change_problem_statement_jobs {

struct ChangeProblemStatementJob {
    decltype(jobs::Job::id) id;
    decltype(internal_files::InternalFile::id) new_statement_file_id;
    sql::fields::Varbinary<256> path_for_new_statement;
};

} // namespace sim::change_problem_statement_jobs

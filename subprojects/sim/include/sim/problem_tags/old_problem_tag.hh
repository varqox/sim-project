#pragma once

// TODO: delete this file when it becomes unneeded

#include <sim/old_sql_fields/bool.hh>
#include <sim/old_sql_fields/varbinary.hh>
#include <sim/primary_key.hh>
#include <sim/problems/old_problem.hh>

namespace sim::problem_tags {

struct OldProblemTag {
    decltype(problems::OldProblem::id) problem_id;
    old_sql_fields::Varbinary<128> name;
    old_sql_fields::Bool is_hidden; // TODO: change is_hidden: Bool, to type: TagType

    static constexpr auto primary_key =
        PrimaryKey{&OldProblemTag::problem_id, &OldProblemTag::name};
};

} // namespace sim::problem_tags

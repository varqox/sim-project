#pragma once

#include "sim/primary_key.hh"
#include "sim/problems/problem.hh"
#include "sim/sql_fields/bool.hh"
#include "sim/sql_fields/varbinary.hh"

namespace sim::problem_tags {

struct ProblemTag {
    decltype(problems::Problem::id) problem_id;
    sql_fields::Varbinary<128> name;
    sql_fields::Bool is_hidden; // TODO: change is_hidden: Bool, to type: TagType

    constexpr static auto primary_key = PrimaryKey{&ProblemTag::problem_id, &ProblemTag::name};
};

} // namespace sim::problem_tags

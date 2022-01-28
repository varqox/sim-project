#pragma once

#include "sim/problems/problem.hh"
#include "sim/sql_fields/bool.hh"
#include "sim/sql_fields/varbinary.hh"

namespace sim::problem_tags {

struct ProblemTag {
    struct Id {
        decltype(problems::Problem::id) problem_id;
        sql_fields::Varbinary<128> name;
    };

    Id id;
    sql_fields::Bool is_hidden; // TODO: change is_hidden: Bool, to type: TagType
};

} // namespace sim::problem_tags

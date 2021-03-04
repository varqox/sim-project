#pragma once

#include "sim/problems/problem.hh"
#include "sim/sql_fields/varchar.hh"

namespace sim::problem_tags {

struct ProblemTag {
    struct Id {
        decltype(problems::Problem::id) problem_id;
        sql_fields::Varchar<128> tag;
    };

    Id id;
    bool hidden;
};

} // namespace sim::problem_tags

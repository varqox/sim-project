#pragma once

#include <sim/problems/problem.hh>
#include <sim/sql/fields/varbinary.hh>

namespace sim::problem_tags {

struct ProblemTag {
    decltype(problems::Problem::id) problem_id;
    sql::fields::Varbinary<128> name;
    bool is_hidden; // TODO: change is_hidden: bool, to type: TagType
};

} // namespace sim::problem_tags

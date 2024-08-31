#include "../progress_printer.hh"
#include "../sim_instance.hh"
#include "problem_tags_merger.hh"
#include "problems_merger.hh"

#include <sim/problem_tags/problem_tag.hh>
#include <sim/sql/sql.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/macros/stack_unwinding.hh>
#include <string>

using sim::problem_tags::ProblemTag;
using sim::sql::InsertInto;
using sim::sql::Select;

namespace mergers {

ProblemTagsMerger::ProblemTagsMerger(
    const SimInstance& main_sim, const SimInstance& other_sim, const ProblemsMerger& problems_merger
)
: main_sim{main_sim}
, other_sim{other_sim}
, problems_merger{problems_merger} {}

void ProblemTagsMerger::merge_and_save() {
    STACK_UNWINDING_MARK;

    // Save other problem_tags
    auto other_problem_tags_copying_progress_printer =
        ProgressPrinter<std::string>{"progress: copying problem_tag:"};
    static_assert(ProblemTag::COLUMNS_NUM == 3, "Update the statements below");
    auto stmt =
        other_sim.mysql.execute(Select("problem_id, is_hidden, name")
                                    .from("problem_tags")
                                    .order_by("problem_id DESC, is_hidden DESC, name DESC"));
    stmt.do_not_store_result(); // minimize memory usage
    ProblemTag problem_tag;
    stmt.res_bind(problem_tag.problem_id, problem_tag.is_hidden, problem_tag.name);
    while (stmt.next()) {
        other_problem_tags_copying_progress_printer.note_id(concat_tostr(
            problem_tag.problem_id,
            ',',
            problem_tag.is_hidden ? "hidden" : "public",
            ',',
            problem_tag.name
        ));
        main_sim.mysql.execute(InsertInto("problem_tags (problem_id, is_hidden, name)")
                                   .values(
                                       "?, ?, ?",
                                       problems_merger.other_id_to_new_id(problem_tag.problem_id),
                                       problem_tag.is_hidden,
                                       problem_tag.name
                                   ));
    }
}

} // namespace mergers

#include "simlib/debug.hh"
#include "src/sip_package.hh"
#include "src/utils.hh"

namespace commands {

std::set<std::string> parse_args_to_solutions(const sim::Simfile& simfile, ArgvParser args) {
    STACK_UNWINDING_MARK;

    std::set<StringView> solutions;
    for (auto const& sol : simfile.solutions) {
        solutions.emplace(sol);
    }
    return files_matching_patterns([solutions = std::move(solutions)](
                                           StringView path) { return solutions.count(path) == 1; },
            args);
}

void prog(ArgvParser args) {
    STACK_UNWINDING_MARK;

    SipPackage sp;
    sp.rebuild_full_simfile();

    std::set<std::string> solutions_to_compile;
    if (args.size() > 0) {
        solutions_to_compile = parse_args_to_solutions(sp.full_simfile, args);
    } else {
        for (auto const& sol : sp.full_simfile.solutions) {
            solutions_to_compile.emplace(sol);
        }
    }

    for (auto const& solution : solutions_to_compile) {
        sp.compile_solution(solution);
    }
}

} // namespace commands

#include "../sip_error.hh"
#include "../sip_package.hh"

#include <simlib/macros/stack_unwinding.hh>

namespace commands {

std::set<std::string> parse_args_to_solutions(const sim::Simfile& simfile, ArgvParser args);

void test(ArgvParser args) {
    STACK_UNWINDING_MARK;

    SipPackage sp;
    sp.rebuild_full_simfile();
    if (sp.full_simfile.solutions.empty()) {
        throw SipError("no solution was found");
    }

    std::set<std::string> solutions_to_test;
    if (args.size() > 0) {
        solutions_to_test = parse_args_to_solutions(sp.full_simfile, args);
    } else {
        solutions_to_test.emplace(sp.full_simfile.solutions.front());
    }

    for (const auto& solution : solutions_to_test) {
        sp.judge_solution(solution);
        // Save limits only if Simfile is already created (because if it creates
        // a Simfile without memory limit it causes sip to fail in the next run)
        if (solution == sp.full_simfile.solutions.front() and access("Simfile", F_OK) == 0) {
            sp.save_limits();
        }
    }
}

} // namespace commands

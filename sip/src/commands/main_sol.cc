#include "simlib/debug.hh"
#include "simlib/file_info.hh"
#include "src/sip_error.hh"
#include "src/sip_package.hh"

namespace commands {

void main_sol(ArgvParser args) {
    STACK_UNWINDING_MARK;

    SipPackage sp;
    if (args.size() > 0) {
        auto new_main_sol = args.extract_next();
        if (access(new_main_sol, F_OK) != 0) {
            throw SipError("solution: ", new_main_sol, " does not exist");
        }

        auto solutions = sp.simfile.config_file().get_var("solutions");
        if (not solutions.is_set()) {
            sp.replace_variable_in_simfile(
                "solutions", std::vector<std::string>{new_main_sol.to_string()});
        } else {
            try {
                sp.simfile.load_solutions();
                auto& sols = sp.simfile.solutions;
                auto it = std::find(sols.begin(), sols.end(), new_main_sol);
                if (it != sols.end()) {
                    sols.erase(it);
                }

                sols.insert(sols.begin(), new_main_sol.to_string());
                sp.replace_variable_in_simfile("solutions", sols);

            } catch (...) {
                sp.replace_variable_in_simfile(
                    "solutions", std::vector<std::string>{new_main_sol.to_string()});
            }
        }
    }

    sp.simfile.load_solutions();
    if (sp.simfile.solutions.empty()) {
        stdlog("no solution is set");
    } else {
        stdlog("main solution = ", sp.simfile.solutions.front());
    }
}

} // namespace commands

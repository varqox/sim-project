#include <simlib/macros/throw.hh>
#include <simlib/overloaded.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/sim/judge/language_suite/suite.hh>

namespace sim::judge::language_suite {

sandbox::result::Ok Suite::await_result(RunHandle&& run_handle) {
    return std::visit(
        overloaded{
            [](sandbox::result::Ok res_ok) { return res_ok; },
            [](sandbox::result::Error res_err) -> sandbox::result::Ok {
                THROW("Suite run error: ", res_err.description);
            }},
        sc.await_result(std::move(run_handle.request_handle))
    );
}

} // namespace sim::judge::language_suite

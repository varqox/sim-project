#include "../sip_package.hh"

#include <simlib/debug.hh>
#include <simlib/random.hh>

namespace commands {

void reseed() {
    STACK_UNWINDING_MARK;

    SipPackage sp;
    try {
        sp.sipfile.load_base_seed(false);
    } catch (...) {
        // Ignore errors
    }

    while (true) {
        using nl = std::numeric_limits<decltype(sp.sipfile.base_seed)>;
        auto new_seed = get_random(nl::min(), nl::max());
        if (new_seed != sp.sipfile.base_seed) {
            sp.sipfile.base_seed = new_seed;
            break;
        }
    }
    sp.replace_variable_in_sipfile(
            "base_seed", intentional_unsafe_string_view(to_string(sp.sipfile.base_seed)));
}

} // namespace commands

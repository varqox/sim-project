#include "../sip_error.hh"
#include "templates.hh"

#include <cstdlib>
#include <simlib/concat.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/file_contents.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/macros/stack_unwinding.hh>
#include <simlib/string_traits.hh>
#include <simlib/string_view.hh>
#include <sys/types.h>
#include <unistd.h>

static auto user_templates_dir() {
    static constexpr StringView templates_home_subdir = "/.config/sip/templates/";
    static auto home = getenv("HOME");
    if (not home) {
        log_warning(
            "failed to get variable HOME from the environment, not "
            "searching for templates in ~",
            templates_home_subdir
        );
    }
    return concat(home, templates_home_subdir);
}

using std::optional;
using std::string;

extern "C" {
extern const unsigned char checker_cc[];
extern const unsigned checker_cc_len;

extern const unsigned char interactive_checker_cc[];
extern const unsigned interactive_checker_cc_len;

extern const unsigned char sim_statement_cls[];
extern const unsigned sim_statement_cls_len;

extern const unsigned char statement_tex[];
extern const unsigned statement_tex_len;

extern const unsigned char gen_cc[];
extern const unsigned gen_cc_len;
}

static string get_template(
    StringView template_name, const unsigned char* default_template, unsigned default_template_len
) {
    static auto templates_dir = user_templates_dir();
    FileDescriptor fd(
        CStringView{from_unsafe{concat_tostr(templates_dir, template_name)}}, O_RDONLY
    );
    if (fd.is_open()) {
        return get_file_contents(fd);
    }

    return {reinterpret_cast<const char*>(default_template), default_template_len};
}

template <class... Subst>
static string replace(StringView text, Subst&&... substitutions) {
    string res;
    for (size_t i = 0; i < text.size(); ++i) {
        auto try_replace = [&](auto& substituton) {
            auto& [pattern, subst] = substituton;
            if (has_prefix(text.substring(i), pattern)) {
                res += subst;
                i += string_length(pattern) - 1;
                return true;
            }

            return false;
        };

        if (not(try_replace(substitutions) or ...)) {
            res += text[i];
        }
    }

    return res;
}

namespace templates {

string checker_cc() { return get_template("checker.cc", ::checker_cc, ::checker_cc_len); }

string interactive_checker_cc() {
    return get_template(
        "interactive_checker.cc", ::interactive_checker_cc, ::interactive_checker_cc_len
    );
}

string sim_statement_cls() {
    return get_template("sim-statement.cls", ::sim_statement_cls, ::sim_statement_cls_len);
}

string statement_tex(StringView problem_name, optional<uint64_t> memory_limit) {
    string full_template_str = get_template("statement.tex", ::statement_tex, ::statement_tex_len);
    return replace(
        full_template_str,
        std::pair("<<<name>>>", problem_name),
        std::pair("<<<mem>>>", (memory_limit ? concat(*memory_limit >> 20, " MiB") : concat()))
    );
}

string gen_cc() { return get_template("gen.cc", ::gen_cc, ::gen_cc_len); }

} // namespace templates

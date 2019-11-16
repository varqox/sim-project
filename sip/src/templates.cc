#include "templates.hh"

#include <climits>
#include <pwd.h>
#include <simlib/debug.hh>
#include <simlib/file_contents.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/string_traits.hh>
#include <sys/types.h>
#include <unistd.h>

static auto user_templates_dir() {
	std::array<char, 1 << 16> buff;
	struct passwd store;
	struct passwd* pw;
	if (getpwuid_r(getuid(), &store, buff.data(), buff.size(), &pw))
		THROW("getpwuid_r()", errmsg());

	if (not pw)
		THROW("getpwuid_r() says you have no passwd record");

	return concat<PATH_MAX>(pw->pw_dir, "/.config/sip/templates/");
}

using std::get;
using std::optional;
using std::pair;
using std::string;

extern "C" {
extern unsigned char checker_cc[];
extern unsigned checker_cc_len;

extern unsigned char interactive_checker_cc[];
extern unsigned interactive_checker_cc_len;

extern unsigned char statement_tex[];
extern unsigned statement_tex_len;
}

static string get_template(StringView template_name,
                           unsigned char* default_template,
                           unsigned default_template_len) {
	static auto templates_dir = user_templates_dir();
	FileDescriptor fd(intentional_unsafe_cstring_view(
	                     concat<PATH_MAX>(templates_dir, template_name)),
	                  O_RDONLY);
	if (fd.is_open())
		return get_file_contents(fd);

	return {(const char*)default_template, default_template_len};
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

		if (not(try_replace(substitutions) or ...))
			res += text[i];
	}

	return res;
}

namespace templates {

string statement_tex(StringView problem_name, optional<uint64_t> memory_limit) {
	string full_template_str =
	   get_template("statement.tex", ::statement_tex, statement_tex_len);
	return replace(
	   full_template_str, pair("<<<name>>>", problem_name),
	   pair("<<<mem>>>",
	        (memory_limit ? concat(*memory_limit >> 20, " MiB") : concat())));
}

string checker_cc() {
	return get_template("checker.cc", ::checker_cc, checker_cc_len);
}

string interactive_checker_cc() {
	return get_template("interactive_checker.cc", ::interactive_checker_cc,
	                    interactive_checker_cc_len);
}

} // namespace templates

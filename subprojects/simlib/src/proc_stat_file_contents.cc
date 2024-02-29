#include <functional>
#include <simlib/concat.hh>
#include <simlib/ctype.hh>
#include <simlib/file_contents.hh>
#include <simlib/proc_stat_file_contents.hh>
#include <simlib/string_traits.hh>

using std::string;

ProcStatFileContents::ProcStatFileContents(string stat_file_contents)
: contents_(std::move(stat_file_contents)) {
    StringView str(contents_);

    // [0] - Process pid
    str.extract_leading(is_space<char>);
    fields_.emplace_back(str.extract_leading(std::not_fn(is_space<char>)));
    // [1] - Executable filename
    str.remove_leading([](int c) { return c != '('; });
    assert(has_prefix(str, "("));
    str.remove_prefix(1);
    fields_.emplace_back(str.extract_prefix(str.rfind(')')));
    assert(has_prefix(str, ")"));
    str.remove_prefix(1);
    // [>1]
    for (;;) {
        str.remove_leading(is_space<char>);
        auto val = str.extract_leading(std::not_fn(is_space<char>));
        if (val.empty()) {
            break;
        }

        fields_.emplace_back(val);
    }
}

ProcStatFileContents ProcStatFileContents::get(pid_t tid) {
    string contents = get_file_contents(concat("/proc/", tid, "/stat"));
    return ProcStatFileContents{contents};
}

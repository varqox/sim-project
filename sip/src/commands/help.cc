#include "../sip_package.hh"

#include <simlib/macros/stack_unwinding.hh>

namespace commands {

void help(const char* program_name) {
    STACK_UNWINDING_MARK;

    if (program_name == nullptr) {
        program_name = "sip";
    }

    printf("Usage: %s [options] <command> [<command args>]\n", program_name);
    puts(R"==(Sip is a tool for preparing and managing Sim problem packages

Commands:
  checker [value]       If [value] is specified: set checker to [value].
                          Otherwise print its current value.
  clean                 Prepare package to archiving: remove unnecessary files
                          (compiled programs, latex logs, etc.).
  devclean              Same as command clean, but also remove all generated
                          tests files.
  devzip                Run devclean command and compress the package into zip
                          (named after the current directory with suffix "-dev")
                          within the upper directory.
  doc [pattern...]      Compile latex statements (if there are any) matching
                          [pattern...] (see Patterns section). No pattern means
                          all TeX files.
  docwatch [pattern...] Like command doc, but watch the files for modification
                          and recompile on any change.
  gen                   Generate tests input and output files
  genin                 Generate tests input files
  genout                Generate tests output files using the main solution
  gentests              Alias to command: gen
  help                  Display this information
  init [directory] [name]
                        Initialize Sip package in [directory] (by default
                          current working directory) if [name] is specified, set
                          problem name to it
  interactive [value]   If [value] is specified: set interactive to [value].
                          Otherwise print its current value
  label [value]         If [value] is specified: set label to [value]. Otherwise
                          print its current value
  main-sol [sol]        If [sol] is specified: set main solution to [sol].
                          Otherwise print main solution
  mem [value]           If [value] is specified: set memory limit to
                          [value] MiB. Otherwise print its current value
  name [value]          If [value] is specified: set name to [value]. Otherwise
                          print its current value
  prog [pattern...]     Compile solutions matching [pattern...]
                          (see Patterns section). No pattern means all solutions.
  regen                 Remove test files that don't belong to either static or
                          generated tests. Then generate tests input and output
                          files
  regenin               Remove test files that don't belong to either static or
                          generated tests. Then generate tests input files
  reseed                Changes base_seed in Sipfile, to a new random value.
  save <args...>        Saves args... Allowed args:
                          scoring - saves scoring to Simfile
  statement [value]     If [value] is specified: set statement to [value].
                          Otherwise print its current value
  templ [names...]      Alias to command template names...
  template [names...]   Save templates of names. Available templates:
                          - statement -- saved to doc/statement.tex
                          - checker -- saved to check/checker.cc
                              There are two checker templates: for interactive
                              and non-interactive problems -- it will be chosen based
                              on 'interactive' property from Simfile.
                          - gen -- saved to utils/gen.cc -- Test generator
                              template.
                          Sip will search for templates in
                          ~/.config/sip/templates/ but if specified template is
                          not there, it will use the default one. Template files
                          should have names: statement.tex, checker.cc,
                          interactive_checker.cc. Default templates can be found
                          here: https://github.com/varqox/sip/tree/master/templates
  test [pattern...]     Run solutions matching [pattern...]
                          (see Patterns section) on tests. No pattern means only
                          main solution. Compiles solutions if necessary.
  unset <names...>      Remove variables names... form Simfile e.g.
                          sip unset label interactive
  version               Display version
  zip                   Run clean command and compress the package into zip
                          (named after the current directory) within the upper
                          directory.

Options:
  -C <directory>        Change working directory to <directory> before doing
                          anything
  -h, --help            Display this information
  -V, --version         Display version
  -v, --verbose         Verbose mode
  -q, --quiet           Quiet mode

Patterns:
  If pattern is a path to file, then only this file matches the pattern.
  Otherwise if the pattern does not contain . (dot) sign then it matches file
  if it is a subsequence of the file's path without extension.
  Otherwise patterns matches file if it is a subsequence of the file's path.
  Examples:
    - "foo/abc" matches "foo/abc", "foo/abc.xyz" but not "foo/ab.c"
    - "cc" matches "foo/collect.c", "abcabc.xyz" but not "abc.c" or "xyz.cc"
    - "a.x" matches "foo/abc.xyz" but not "foo/abcxyz.c" or "by.abcxyz"
    - "o.c" matches "foo/x.c" and "bar/o.c"

Sip package tree:
   main/                Main package folder
   |-- check/           Checker folder - holds checker
   |-- doc/             Documents folder - holds problem statement, elaboration,
   |                      etc.
   |-- prog/            Solutions folder - holds solutions
   |-- in/              Tests' input files folder - holds tests input files
   |-- out/             Tests' output files folder - holds tests output files
   |-- utils/           Utilities folder - holds test input generators, input
   |                      verifiers and other files used by Sip
   |-- Simfile          Simfile - holds package primary config
   `-- Sipfile          Sip file - holds Sip configuration and rules for
                          generating test inputs)==");
}

} // namespace commands

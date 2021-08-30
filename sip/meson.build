project('sip',
    ['c', 'cpp'],
    license: 'MIT',
    meson_version : '>=0.52.1',
    default_options : meson.get_cross_property('project_configuration', [
        'cpp_std=c++17',
        'warning_level=3',
    ])
)

cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

if get_option('warning_level') > '0'
    warnings = [
        '-Wshadow',
        '-Wunreachable-code',
        '-Wdocumentation',
        '-Wgnu',
        '-Wunused-lambda-capture',
    ]
    foreach warning : warnings
        if cc.has_argument(warning)
            add_project_arguments(warning, language : 'c')
        endif
        if cpp.has_argument(warning)
            add_project_arguments(warning, language : 'cpp')
        endif
    endforeach
endif

static_kwargs = {}
static_dep = declare_dependency()
if get_option('static')
    if get_option('default_library') != 'static'
        error('-Dstatic=true requires -Ddefault_library=static')
    endif
    static_kwargs = {'static': true}
    static_dep = declare_dependency(link_args : '-static')
endif

dump_file_as_c_array = generator(find_program('subprojects/simlib/dump_file_as_c_array.py'),
    output : ['@BASENAME@_dump.c'],
    arguments : ['@INPUT@', '@EXTRA_ARGS@'],
    capture : true,
)

simlib_proj = subproject('simlib')
simlib_dep = simlib_proj.get_variable('simlib_dep')

if host_machine.cpu_family() == 'x86'
    proot_bin = 'bin/proot-x86'
elif host_machine.cpu_family() == 'x86_64'
    proot_bin = 'bin/proot-x86_64'
else
    error('This CPU architecture is not supported')
endif

sip = executable('sip',
    sources : [
        'src/commands/checker.cc',
        'src/commands/clean.cc',
        'src/commands/devclean.cc',
        'src/commands/devzip.cc',
        'src/commands/doc.cc',
        'src/commands/docwatch.cc',
        'src/commands/gen.cc',
        'src/commands/genin.cc',
        'src/commands/genout.cc',
        'src/commands/help.cc',
        'src/commands/init.cc',
        'src/commands/interactive.cc',
        'src/commands/label.cc',
        'src/commands/main_sol.cc',
        'src/commands/mem.cc',
        'src/commands/name.cc',
        'src/commands/prog.cc',
        'src/commands/regen.cc',
        'src/commands/regenin.cc',
        'src/commands/reseed.cc',
        'src/commands/save.cc',
        'src/commands/statement.cc',
        'src/commands/template.cc',
        'src/commands/test.cc',
        'src/commands/unset.cc',
        'src/commands/version.cc',
        'src/commands/zip.cc',
        'src/compilation_cache.cc',
        'src/main.cc',
        'src/sip_package.cc',
        'src/sipfile.cc',
        'src/templates/templates.cc',
        'src/tests_files.cc',
        'src/utils.cc',
        dump_file_as_c_array.process(
            proot_bin,
            extra_args : 'proot_dump'
        ),
        dump_file_as_c_array.process(
            'templates/checker.cc',
            extra_args : 'checker_cc'
        ),
        dump_file_as_c_array.process(
            'templates/interactive_checker.cc',
            extra_args : 'interactive_checker_cc'
        ),
        dump_file_as_c_array.process(
            'templates/sim-statement.cls',
            extra_args : 'sim_statement_cls'
        ),
        dump_file_as_c_array.process(
            'templates/statement.tex',
            extra_args : 'statement_tex'
        ),
        dump_file_as_c_array.process(
            'templates/gen.cc',
            extra_args : 'gen_cc'
        ),
        custom_target('git_commit.cc',
            command : find_program('src/gen_git_commit_cc.py'),
            depend_files : '.git/logs/HEAD',
            output : 'git_commit.cc',
            capture : true,
        ),
    ],
    dependencies : [
        simlib_dep,
        static_dep,
    ],
    install : true,
    install_rpath : get_option('prefix') / get_option('libdir'),
)

base_targets = [sip]
alias_target('base', base_targets)
run_target('format', command : [find_program('format.py'), meson.current_source_dir()])
run_target('tidy', command : [find_program('tidy')], depends : base_targets)

#################################### Tests ####################################
test('sip_tests', find_program('test/run_sip_tests.py'), args : [sip.full_path()], priority : 10)

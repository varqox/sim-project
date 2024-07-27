project('sim',
    ['c', 'cpp'],
    license: 'MIT',
    meson_version : '>=0.58',
    default_options : meson.get_external_property('project_configuration', [
        'c_std=gnu17',
        'cpp_std=gnu++17',
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
        '-Wno-c++20-designator',
        '-Wno-c++20-extensions',
        '-Wno-gnu-zero-variadic-macro-arguments',
        '-Wno-c++2b-extensions',
        '-Wuninitialized',
        '-Wnon-virtual-dtor',
        '-Woverloaded-virtual',
        '-Wduplicated-cond',
        '-Wduplicated-branches',
        '-Wlogical-op',
        '-Wnull-dereference',
        '-Wformat=2',
        '-Wlifetime',
    ]
    foreach warning : warnings
        if cc.has_argument(warning)
            add_project_arguments(warning, language : 'c')
        endif
        if cpp.has_argument(warning)
            add_project_arguments(warning, language : 'cpp')
        endif
    endforeach

    extra_gcc_warnings = [
        '-Wno-shadow',
        '-Wno-pedantic',
    ]
    foreach warning : extra_gcc_warnings
        if cc.get_id() == 'gcc'
            add_project_arguments(warning, language : 'c')
        endif
        if cpp.get_id() == 'gcc'
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

_install = get_option('install')

mariadb_dep = dependency('libmariadb')

simlib_proj = subproject(
    'simlib',
    default_options : [
        'install=' + (_install ? 'true' : 'false'),
    ],
)
simlib_dep = simlib_proj.get_variable('simlib_dep')

libsim_dependencies = [
    simlib_dep,
    mariadb_dep,
]

libsim_incdir = include_directories('include', is_system : false)
libsim = library('sim',
    implicit_include_directories : false,
    include_directories : libsim_incdir,
    sources : [
        'src/sim/contest_files/permissions.cc',
        'src/sim/contests/permissions.cc',
        'src/sim/cpp_syntax_highlighter.cc',
        'src/sim/db/schema.cc',
        'src/sim/db/tables.cc',
        'src/sim/jobs/utils.cc',
        'src/sim/merging/merge_ids.cc',
        'src/sim/mysql/mysql.cc',
        'src/sim/problems/permissions.cc',
        'src/sim/random.cc',
        'src/sim/submissions/update_final.cc',
        'src/sim/users/user.cc',
    ],
    dependencies : libsim_dependencies,
    install : _install,
)

libsim_dep = declare_dependency(
    include_directories : libsim_incdir,
    dependencies : libsim_dependencies,
    link_with : libsim,
)

job_server = executable('job-server',
    implicit_include_directories : false,
    sources : [
        'src/job_server/job_handlers/add_or_reupload_problem/add_or_reupload_problem.cc',
        'src/job_server/job_handlers/add_problem.cc',
        'src/job_server/job_handlers/change_problem_statement.cc',
        'src/job_server/job_handlers/common.cc',
        'src/job_server/job_handlers/delete_contest.cc',
        'src/job_server/job_handlers/delete_contest_problem.cc',
        'src/job_server/job_handlers/delete_contest_round.cc',
        'src/job_server/job_handlers/delete_internal_file.cc',
        'src/job_server/job_handlers/delete_problem.cc',
        'src/job_server/job_handlers/delete_user.cc',
        'src/job_server/job_handlers/judge_or_rejudge_submission.cc',
        'src/job_server/job_handlers/merge_problems.cc',
        'src/job_server/job_handlers/merge_users.cc',
        'src/job_server/job_handlers/reselect_final_submissions_in_contest_problem.cc',
        'src/job_server/job_handlers/reset_problem_time_limits.cc',
        'src/job_server/job_handlers/reupload_problem.cc',
        'src/job_server/main.cc',
    ],
    dependencies : [
        libsim_dep,
        static_dep,
    ],
    install : _install,
    install_rpath : get_option('prefix') / get_option('libdir'),
)

sim_merger = executable('sim-merger',
    implicit_include_directories : false,
    sources : [
        'src/sim_merger/sim_merger.cc',
    ],
    dependencies : [
        libsim_dep,
        static_dep,
    ],
    install : _install,
    install_rpath : get_option('prefix') / get_option('libdir'),
)

sim_upgrader = executable('sim-upgrader',
    implicit_include_directories : false,
    sources : [
        'src/sim_upgrader.cc',
    ],
    dependencies : [
        libsim_dep,
        static_dep,
    ],
    install : _install,
    install_rpath : get_option('prefix') / get_option('libdir'),
)

setup_installation = executable('setup-installation',
    implicit_include_directories : false,
    sources : [
        'src/setup_installation.cc',
    ],
    dependencies : [
        libsim_dep,
        static_dep,
    ],
    install : false,
    install_rpath : get_option('prefix') / get_option('libdir'),
)

backup = executable('backup',
    implicit_include_directories : false,
    sources : [
        'src/backup.cc',
    ],
    dependencies : [
        libsim_dep,
        static_dep,
    ],
    install : _install,
    install_rpath : get_option('prefix') / get_option('libdir'),
)

restore_backup = executable('restore-backup',
    implicit_include_directories : false,
    sources : [
        'src/restore_backup.cc',
    ],
    dependencies : [
        libsim_dep,
        static_dep,
    ],
    install : _install,
    install_rpath : get_option('prefix') / get_option('libdir'),
)

sim_server = executable('sim-server',
    implicit_include_directories : false,
    sources : [
        'src/web_server/capabilities/contest.cc',
        'src/web_server/capabilities/contest_entry_token.cc',
        'src/web_server/capabilities/contests.cc',
        'src/web_server/capabilities/jobs.cc',
        'src/web_server/capabilities/logs.cc',
        'src/web_server/capabilities/problems.cc',
        'src/web_server/capabilities/submissions.cc',
        'src/web_server/capabilities/users.cc',
        'src/web_server/contest_entry_tokens/api.cc',
        'src/web_server/contest_entry_tokens/ui.cc',
        'src/web_server/http/cookies.cc',
        'src/web_server/http/request.cc',
        'src/web_server/http/response.cc',
        'src/web_server/old/api.cc',
        'src/web_server/old/contest_files.cc',
        'src/web_server/old/contest_files_api.cc',
        'src/web_server/old/contest_users_api.cc',
        'src/web_server/old/contests.cc',
        'src/web_server/old/contests_api.cc',
        'src/web_server/old/jobs.cc',
        'src/web_server/old/jobs_api.cc',
        'src/web_server/old/problems.cc',
        'src/web_server/old/problems_api.cc',
        'src/web_server/old/session.cc',
        'src/web_server/old/sim.cc',
        'src/web_server/old/submissions.cc',
        'src/web_server/old/submissions_api.cc',
        'src/web_server/old/template.cc',
        'src/web_server/old/users.cc',
        'src/web_server/problems/api.cc',
        'src/web_server/problems/ui.cc',
        'src/web_server/server/connection.cc',
        'src/web_server/server/server.cc',
        'src/web_server/ui/ui.cc',
        'src/web_server/ui_template.cc',
        'src/web_server/users/api.cc',
        'src/web_server/users/ui.cc',
        'src/web_server/web_worker/context.cc',
        'src/web_server/web_worker/web_worker.cc',
    ],
    dependencies : [
        libsim_dep,
        static_dep,
    ],
    install : _install,
    install_rpath : get_option('prefix') / get_option('libdir'),
)

manage = executable('manage',
    implicit_include_directories : false,
    sources : [
        'src/manage.cc'
    ],
    dependencies : [
        simlib_dep,
        static_dep,
    ],
    install : _install,
    install_rpath : get_option('prefix') / get_option('libdir'),
    install_dir : '.',
)

base_targets = [
    backup,
    job_server,
    libsim,
    manage,
    restore_backup,
    setup_installation,
    sim_merger,
    sim_server,
    sim_upgrader,
]
alias_target('base', base_targets)

################################# Installation #################################

if _install
    install_subdir('src/web_server/static', install_dir : '.')

    mkdir_p = 'mkdir -p "$MESON_INSTALL_DESTDIR_PREFIX/@0@"'
    cp_if_missing = 'test -e "$MESON_INSTALL_DESTDIR_PREFIX/@1@" || cp "' + meson.current_source_dir() + '/@0@" "$MESON_INSTALL_DESTDIR_PREFIX/@1@"'
    meson.add_install_script('sh', '-c', 'chmod 0700 "$MESON_INSTALL_DESTDIR_PREFIX"')
    meson.add_install_script('sh', '-c', mkdir_p.format('internal_files'))
    meson.add_install_script('sh', '-c', mkdir_p.format('logs'))
    meson.add_install_script('sh', '-c', cp_if_missing.format('src/sim.conf', 'sim.conf'))
    meson.add_install_script('sh', '-c', 'if test -e "$MESON_INSTALL_DESTDIR_PREFIX/.db.config"; then ' + setup_installation.full_path() + ' "$MESON_INSTALL_DESTDIR_PREFIX"; else { printf "\\033[1;33mTo complete installation you need to run:\\n%s %s\\033[m\\n" \'' + setup_installation.full_path() + '\' $MESON_INSTALL_DESTDIR_PREFIX; false; } fi')
endif

#################################### Tests ####################################

gtest_main_dep = simlib_proj.get_variable('gtest_main_dep')
gmock_dep = simlib_proj.get_variable('gmock_dep')

tests = {
    'test/sim/cpp_syntax_highlighter.cc': {'args': [meson.current_source_dir() + '/test/sim/cpp_syntax_highlighter_test_cases/']},
    'test/sim/merging/merge_ids.cc': {'priority': 10},
    'test/sim/sql/sql.cc': {},
    'test/web_server/http/form_validation.cc': {},
}

foreach test_src, args : tests
    test_executable_deps = ['tester' in args ? gtest_dep : gtest_main_dep]
    tester_dep = []
    test_kwargs = {}
    foreach key, value : args
        if key == 'dependencies'
            test_executable_deps = value
        elif key == 'tester'
            tester = executable(value.underscorify(),
                implicit_include_directories : false,
                sources : value,
                build_by_default : false,
                dependencies : simlib_dep,
            )
            tester_dep = [tester]
            test_kwargs += {
                'args': [tester.full_path()],
            }
        else
            test_kwargs += {key: value}
        endif
    endforeach
    test(test_src.replace('test/', '').replace('.cc', ''),
        executable(test_src.underscorify(),
            implicit_include_directories : false,
            sources : test_src,
            dependencies : [
                simlib_dep,
                libsim_dep,
                test_executable_deps,
            ],
            build_by_default : false,
        ),
        depends : tester_dep,
        timeout : 300,
        kwargs : test_kwargs,
    )
endforeach

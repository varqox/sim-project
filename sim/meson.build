project('sim',
    ['c', 'cpp'],
    license: 'MIT',
    meson_version : '>=0.52.1',
    default_options : meson.get_cross_property('project_configuration', [
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
        '-Wno-gnu-zero-variadic-macro-arguments',
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

mariadb_dep = dependency('mariadb')

simlib_proj = subproject('simlib')
simlib_dep = simlib_proj.get_variable('simlib_dep')

if host_machine.cpu_family() == 'x86'
    proot_bin = 'bin/proot-x86'
elif host_machine.cpu_family() == 'x86_64'
    proot_bin = 'bin/proot-x86_64'
else
    error('This CPU architecture is not supported')
endif

libsim_dependencies = [
    simlib_dep,
    mariadb_dep,
]

libsim_incdir = include_directories('include', is_system : false)
libsim = library('sim',
    sources : [
        'src/sim/contest_files/permissions.cc',
        'src/sim/contests/permissions.cc',
        'src/sim/cpp_syntax_highlighter.cc',
        'src/sim/jobs/utils.cc',
        'src/sim/mysql/mysql.cc',
        'src/sim/problems/permissions.cc',
        'src/sim/random.cc',
        'src/sim/submissions/update_final.cc',
        'src/sim/users/user.cc',
    ],
    include_directories : libsim_incdir,
    dependencies : libsim_dependencies,
    install : true,
)

libsim_dep = declare_dependency(
    include_directories : libsim_incdir,
    link_with : libsim,
    dependencies : libsim_dependencies,
)

job_server = executable('job-server',
    sources : [
        'src/job_server/dispatcher.cc',
        'src/job_server/job_handlers/add_or_reupload_problem__judge_main_solution_base.cc',
        'src/job_server/job_handlers/add_or_reupload_problem_base.cc',
        'src/job_server/job_handlers/add_problem.cc',
        'src/job_server/job_handlers/change_problem_statement.cc',
        'src/job_server/job_handlers/delete_contest.cc',
        'src/job_server/job_handlers/delete_contest_problem.cc',
        'src/job_server/job_handlers/delete_contest_round.cc',
        'src/job_server/job_handlers/delete_internal_file.cc',
        'src/job_server/job_handlers/delete_problem.cc',
        'src/job_server/job_handlers/delete_user.cc',
        'src/job_server/job_handlers/job_handler.cc',
        'src/job_server/job_handlers/judge_base.cc',
        'src/job_server/job_handlers/judge_or_rejudge.cc',
        'src/job_server/job_handlers/merge_problems.cc',
        'src/job_server/job_handlers/merge_users.cc',
        'src/job_server/job_handlers/reselect_final_submissions_in_contest_problem.cc',
        'src/job_server/job_handlers/reset_problem_time_limits.cc',
        'src/job_server/job_handlers/reset_time_limits_in_problem_package_base.cc',
        'src/job_server/job_handlers/reupload_problem.cc',
        'src/job_server/main.cc',
    ],
    dependencies : [
        libsim_dep,
        static_dep,
    ],
    install : true,
    install_rpath : get_option('prefix') / get_option('libdir'),
)

sim_merger = executable('sim-merger',
    sources : [
        'src/sim_merger/sim_merger.cc',
    ],
    dependencies : [
        libsim_dep,
        static_dep,
    ],
    install : true,
    install_rpath : get_option('prefix') / get_option('libdir'),
)

sim_upgrader = executable('sim-upgrader',
    sources : [
        'src/sim_upgrader.cc',
    ],
    dependencies : [
        libsim_dep,
        static_dep,
    ],
    install : false,
    install_rpath : get_option('prefix') / get_option('libdir'),
)

setup_installation = executable('setup-installation',
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
    sources : [
        'src/backup.cc',
    ],
    dependencies : [
        libsim_dep,
        static_dep,
    ],
    install : true,
    install_rpath : get_option('prefix') / get_option('libdir'),
)

sim_server = executable('sim-server',
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
    install : true,
    install_rpath : get_option('prefix') / get_option('libdir'),
)

manage = executable('manage',
    sources : [
        'src/manage.cc'
    ],
    dependencies : [
        simlib_dep,
        static_dep,
    ],
    install : true,
    install_rpath : get_option('prefix') / get_option('libdir'),
    install_dir : '.',
)

base_targets = [
    backup,
    job_server,
    libsim,
    manage,
    setup_installation,
    sim_merger,
    sim_server,
    sim_upgrader,
]
alias_target('base', base_targets)
run_target('format', command : [find_program('format.py'), meson.current_source_dir()])
run_target('tidy', command : [find_program('tidy')], depends : base_targets)

################################# Installation #################################

install_subdir('src/web_server/static', install_dir : '.')

if host_machine.cpu_family() == 'x86'
    proot_bin = 'bin/proot-x86'
elif host_machine.cpu_family() == 'x86_64'
    proot_bin = 'bin/proot-x86_64'
else
    error('This CPU architecture is not supported')
endif
install_data(proot_bin, install_dir : '.', rename : 'proot')

mkdir_p = 'mkdir -p "$MESON_INSTALL_DESTDIR_PREFIX/@0@"'
cp_n = 'cp -n "$MESON_SOURCE_ROOT/@0@" "$MESON_INSTALL_DESTDIR_PREFIX/@1@"'
meson.add_install_script('sh', '-c', 'chmod 0700 "$MESON_INSTALL_DESTDIR_PREFIX"')
meson.add_install_script('sh', '-c', mkdir_p.format('internal_files'))
meson.add_install_script('sh', '-c', mkdir_p.format('logs'))
meson.add_install_script('sh', '-c', cp_n.format('src/sim.conf', 'sim.conf'))
meson.add_install_script('sh', '-c', setup_installation.full_path() + ' "$MESON_INSTALL_DESTDIR_PREFIX"')

#################################### Tests ####################################

gtest_main_dep = simlib_proj.get_variable('gtest_main_dep')
gmock_dep = simlib_proj.get_variable('gmock_dep')

tests = [
    ['test/sim/cpp_syntax_highlighter.cc', [], {}],
    ['test/sim/jobs/utils.cc', [], {}],
    ['test/web_server/http/form_validation.cc', [], {}],
]
foreach test : tests
    name = test[0].underscorify()
    exe = executable(name, sources : test[0], dependencies : [
        gtest_main_dep,
        libsim_dep,
        test[1],
    ], build_by_default : false)
    test(name, exe, timeout : 300, kwargs : test[2], workdir : meson.current_source_dir())
endforeach

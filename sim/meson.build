project('sim',
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
if get_option('static')
    static_kwargs = {'static': true}
    add_project_link_arguments(['-static'], language : ['c', 'cpp'])
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

libsim_incdir = include_directories('src/include', is_system : false)
libsim = library('sim',
    sources : [
        'src/lib/contest_file_permissions.cc',
        'src/lib/contest_permissions.cc',
        'src/lib/cpp_syntax_highlighter.cc',
        'src/lib/jobs.cc',
        'src/lib/mysql.cc',
        'src/lib/problem_permissions.cc',
        'src/lib/random.cc',
        'src/lib/submission.cc',
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
    ],
    install : true,
    install_rpath : get_option('prefix') / get_option('libdir'),
)

sim_server = executable('sim-server',
    sources : [
        'src/web_interface/api.cc',
        'src/web_interface/connection.cc',
        'src/web_interface/contest_entry_token_api.cc',
        'src/web_interface/contest_users_api.cc',
        'src/web_interface/contests.cc',
        'src/web_interface/contests_api.cc',
        'src/web_interface/contest_files.cc',
        'src/web_interface/contest_files_api.cc',
        'src/web_interface/http_request.cc',
        'src/web_interface/http_response.cc',
        'src/web_interface/jobs.cc',
        'src/web_interface/jobs_api.cc',
        'src/web_interface/problems.cc',
        'src/web_interface/problems_api.cc',
        'src/web_interface/server.cc',
        'src/web_interface/session.cc',
        'src/web_interface/sim.cc',
        'src/web_interface/submissions.cc',
        'src/web_interface/submissions_api.cc',
        'src/web_interface/template.cc',
        'src/web_interface/users.cc',
        'src/web_interface/users_api.cc',
    ],
    dependencies : [
        libsim_dep,
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

install_subdir('src/static', install_dir : '.')

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
    ['test/jobs.cc', [], {}],
    ['test/cpp_syntax_highlighter.cc', [], {}],
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

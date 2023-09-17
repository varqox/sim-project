project('simlib',
    ['c', 'cpp'],
    license: 'MIT',
    meson_version : '>=0.58',
    default_options : meson.get_external_property(
        'project_configuration',
        [
            'c_std=gnu17',
            'cpp_std=gnu++17',
            'warning_level=3',
        ]
    )
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
        '-Wno-c++2b-extensions',
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
if get_option('static')
    if get_option('default_library') != 'static'
        error('-Dstatic=true requires -Ddefault_library=static')
    endif
    static_kwargs = {'static': true}
endif

dump_file_as_c_array = executable('dump_file_as_c_array',
    implicit_include_directories : false,
    sources : 'src/dump_file_as_c_array.c',
    install : false,
)

dump_file_as_c_array_gen = generator(dump_file_as_c_array,
    output : ['@BASENAME@_dump.c'],
    arguments : ['@INPUT@', '@EXTRA_ARGS@'],
    capture : true,
)

dl_dep = cpp.find_library('dl', kwargs : static_kwargs)
libcap_dep = dependency('libcap', kwargs : static_kwargs)
libseccomp_dep = dependency('libseccomp', kwargs : static_kwargs)
libzip_dep = dependency('libzip', version : '>=1.3.0', kwargs : static_kwargs)
rt_dep = cpp.find_library('rt', kwargs : static_kwargs)

threads_dep = dependency('threads', kwargs : static_kwargs)
if get_option('static')
    threads_dep = declare_dependency(
        compile_args : '-pthread',
        link_args : ['-Wl,--whole-archive', '-lpthread', '-Wl,--no-whole-archive'],
    )
endif

simlib_incdir = include_directories('include', is_system : false)
simlib_dependencies = [
    dl_dep,
    libseccomp_dep,
    libzip_dep,
    rt_dep,
    threads_dep,
    declare_dependency(
        include_directories : include_directories('3rdparty', is_system : true),
    ),
]

sandbox_supervisor = executable('sandbox_supervisor',
    implicit_include_directories : false,
    include_directories : simlib_incdir,
    sources : [
        'src/file_contents.cc',
        'src/sandbox/pid1/pid1.cc',
        'src/sandbox/si.cc',
        'src/sandbox/supervisor/request/deserialize.cc',
        'src/sandbox/supervisor/supervisor.cc',
        'src/sandbox/tracee/tracee.cc',
        'src/socket_stream_ext.cc',
        'src/write_exact.cc',
    ],
    dependencies : [
        libcap_dep,
        libseccomp_dep,
    ],
    install : false,
)

sandbox_supervisor_dump_c = custom_target('sandbox_supervisor_dump',
    command : [
        dump_file_as_c_array,
        '@INPUT@',
        'sandbox_supervisor_dump',
    ],
    input : sandbox_supervisor,
    output : 'sandbox_supervisor_dump.c',
    capture : true,
)

simlib = library('simlib',
    implicit_include_directories : false,
    include_directories : simlib_incdir,
    sources : [
        'src/aho_corasick.cc',
        'src/config_file.cc',
        'src/event_queue.cc',
        'src/file_contents.cc',
        'src/file_manip.cc',
        'src/http/response.cc',
        'src/humanize.cc',
        'src/inotify.cc',
        'src/libarchive_zip.cc',
        'src/logger.cc',
        'src/old_sandbox.cc',
        'src/path.cc',
        'src/proc_stat_file_contents.cc',
        'src/proc_status_file.cc',
        'src/process.cc',
        'src/random.cc',
        'src/read_exact.cc',
        'src/sandbox/client/request/serialize.cc',
        'src/sandbox/sandbox.cc',
        'src/sandbox/seccomp/allow_common_safe_syscalls.cc',
        'src/sandbox/si.cc',
        'src/sha.cc',
        'src/sim/checker.cc',
        'src/sim/compile.cc',
        'src/sim/conver.cc',
        'src/sim/judge/language_suite/bash.cc',
        'src/sim/judge/language_suite/c_clang.cc',
        'src/sim/judge/language_suite/c_gcc.cc',
        'src/sim/judge/language_suite/cpp_clang.cc',
        'src/sim/judge/language_suite/cpp_gcc.cc',
        'src/sim/judge/language_suite/fully_compiled_language.cc',
        'src/sim/judge/language_suite/fully_interpreted_language.cc',
        'src/sim/judge/language_suite/pascal.cc',
        'src/sim/judge/language_suite/python.cc',
        'src/sim/judge/language_suite/rust.cc',
        'src/sim/judge/language_suite/suite.cc',
        'src/sim/judge/test_on_interactive_test.cc',
        'src/sim/judge/test_on_test.cc',
        'src/sim/judge_worker.cc',
        'src/sim/problem_package.cc',
        'src/sim/simfile.cc',
        'src/socket_stream_ext.cc',
        'src/spawner.cc',
        'src/string_compare.cc',
        'src/temporary_directory.cc',
        'src/temporary_file.cc',
        'src/time.cc',
        'src/unlinked_temporary_file.cc',
        'src/working_directory.cc',
        'src/write_exact.cc',
        sandbox_supervisor_dump_c,
        dump_file_as_c_array_gen.process(
            'src/sim/default_checker.c',
            extra_args : 'default_checker_c'
        ),
    ],
    dependencies : simlib_dependencies,
    install : true,
)

simlib_dep = declare_dependency(
    include_directories : simlib_incdir,
    dependencies : simlib_dependencies,
    link_with : simlib,
)

examples = [
    executable('sandbox_time',
        implicit_include_directories : false,
        sources : [
            'examples/sandbox/time.cc',
        ],
        dependencies : [
            simlib_dep,
        ],
        install : false,
    ),
    executable('sandbox_bench',
        implicit_include_directories : false,
        sources : [
            'examples/sandbox/bench.cc',
        ],
        dependencies : [
            simlib_dep,
        ],
        install : false,
    ),
]

alias_target('base', simlib)
alias_target('examples', examples)

run_target('format', command : [find_program('format.py'), meson.current_source_dir()])
run_target('tidy', command : [find_program('tidy')], depends : [simlib, examples])

#################################### Tests ####################################

# Compiling with dependency('gtest', main : true, fallback : ['gtest', 'gtest_main_dep'])
# is crap because it compiles whole googletest source code every time a test executable
# is built if the fallback (gtest subproject) is chosen.
#
# To solve this, we build gtest/gmock as a shared library (I see no reason for it to be
# a static library) and link test executables against it instead of compiling whole gtest
# every time a test executable is compiled.
#
# 'pkg-config' method is used, as 'system' does the same crap descripted above e.g. on
# Debian 11 when 'googletest' is installed but 'libgtest-dev' is not installed. pkg-config
# does the right thing and detects the system to not provide gtest library.
gtest_main_dep = dependency('gtest', main : true, method : 'pkg-config', required : false)
if not gtest_main_dep.found()
    gtest_main_dep = subproject('gtest').get_variable('gtest_main_dep')
    gtest_main_dep = declare_dependency(
        dependencies : gtest_main_dep.partial_dependency(includes : true),
        link_with : shared_library('gtest_main',
            implicit_include_directories : false,
            dependencies : gtest_main_dep,
        ),
    )
endif
gtest_dep = dependency('gtest', main : false, method : 'pkg-config', required : false)
if not gtest_dep.found()
    gtest_dep = subproject('gtest').get_variable('gtest_dep')
    gtest_dep = declare_dependency(
        dependencies : gtest_dep.partial_dependency(includes : true),
        link_with : shared_library('gtest',
            implicit_include_directories : false,
            dependencies : gtest_dep,
        ),
    )
endif
gmock_dep = dependency('gmock', main : false, method : 'pkg-config', required : false)
if not gmock_dep.found()
    gmock_dep = subproject('gtest').get_variable('gmock_dep')
    gmock_dep = declare_dependency(
        dependencies : gmock_dep.partial_dependency(includes : true),
        link_with : shared_library('gmock',
            implicit_include_directories : false,
            dependencies : gmock_dep,
        ),
    )
endif

tests = {
    'test/argv_parser.cc': {},
    'test/array_buff.cc': {},
    'test/array_vec.cc': {},
    'test/call_in_destructor.cc': {},
    'test/concat.cc': {},
    'test/concat_common.cc': {},
    'test/concat_tostr.cc': {},
    'test/config_file.cc': {'priority': 10},
    'test/constructor_traits.cc': {},
    'test/contains.cc': {'dependencies' : []},
    'test/conver.cc': {'priority': 10, 'args': meson.current_source_dir() + '/test/conver_test_cases/'},
    'test/converts_safely_to.cc': {'dependencies' : []},
    'test/ctype.cc': {},
    'test/defer.cc': {},
    'test/deserialize.cc': {},
    'test/directory.cc': {},
    'test/enum_val.cc': {},
    'test/err_defer.cc': {},
    'test/errmsg.cc': {},
    'test/event_queue.cc': {},
    'test/fd_pread_buff.cc': {},
    'test/file_contents.cc': {},
    'test/file_descriptor.cc': {},
    'test/file_info.cc': {},
    'test/file_manip.cc': {},
    'test/file_path.cc': {},
    'test/from_unsafe.cc': {},
    'test/http/response.cc': {},
    'test/http/url_dispatcher.cc': {},
    'test/humanize.cc': {},
    'test/inotify.cc': {},
    'test/inplace_array.cc': {},
    'test/inplace_buff.cc': {},
    'test/json_str/json_str.cc': {},
    'test/libzip.cc': {},
    'test/logger.cc': {},
    'test/macros/log_line.cc': {},
    'test/macros/macros.cc': {},
    'test/macros/stack_unwinding.cc': {},
    'test/macros/stringify.cc': {},
    'test/macros/throw.cc': {},
    'test/macros/wont_throw.cc': {},
    'test/member_comparator.cc': {},
    'test/memory.cc': {},
    'test/merge.cc': {},
    'test/mysql/mysql.cc': {},
    'test/noexcept_concat.cc': {},
    'test/old_sandbox.cc': {'priority': 8, 'args': meson.current_source_dir() + '/test/old_sandbox_test_cases/'},
    'test/opened_temporary_file.cc': {'dependencies': [gtest_main_dep, gmock_dep]},
    'test/path.cc': {},
    'test/proc_stat_file_contents.cc': {},
    'test/proc_status_file.cc': {},
    'test/process.cc': {},
    'test/random.cc': {},
    'test/ranges.cc': {},
    'test/read_bytes_as.cc': {},
    'test/read_exact.cc': {},
    'test/request_uri_parser.cc': {},
    'test/result.cc': {},
    'test/sandbox/cancelling_requests.cc': {},
    'test/sandbox/destructor.cc': {},
    'test/sandbox/external/broken_pipe.cc': {},
    'test/sandbox/external/capabilities_and_user_namespaces.cc': {'dependencies': [gtest_main_dep, libcap_dep]},
    'test/sandbox/external/detect_parent_process_death_using_pidfd.cc': {},
    'test/sandbox/external/poll_hup_on_sock_seqpacket.cc': {},
    'test/sandbox/external/poll_hup_on_sock_stream.cc': {},
    'test/sandbox/external/poll_pidfd.cc': {},
    'test/sandbox/external/polling_signalfd_sigchld_and_waitpid_work.cc': {},
    'test/sandbox/external/sigpipe_send_recv.cc': {},
    'test/sandbox/invalid_request.cc': {},
    'test/sandbox/killing_request.cc': {},
    'test/sandbox/sandbox_closes_std_file_descriptors_after_spawning_tracee.cc': {'tester': 'test/sandbox/sandbox_closes_std_file_descriptors_after_spawning_tracee_tester.cc'},
    'test/sandbox/sandbox_closes_unnecessary_file_descriptors.cc': {'tester': 'test/sandbox/sandbox_closes_unnecessary_file_descriptors_tester.cc'},
    'test/sandbox/sandbox_mount_namespace.cc': {'tester': 'test/sandbox/sandbox_mount_namespace_tester.cc', 'priority': 10},
    'test/sandbox/sandbox_opens_dev_null_as_default_stdin_stdout_stderr.cc': {'tester': 'test/sandbox/sandbox_opens_dev_null_as_default_stdin_stdout_stderr_tester.cc'},
    'test/sandbox/sandbox_places_tracee_in_a_separate_session_and_process_group.cc': {'tester': 'test/sandbox/sandbox_places_tracee_in_a_separate_session_and_process_group_tester.cc'},
    'test/sandbox/sandbox_reports_unexpected_supervisor_process_death.cc': {},
    'test/sandbox/sandbox_supervisor_does_not_run_as_root.cc': {'dependencies': [gtest_dep], 'args': sandbox_supervisor},
    'test/sandbox/sandbox_supervisor_validates_arguments.cc': {'dependencies': [gtest_dep], 'args': sandbox_supervisor},
    'test/sandbox/sandbox_terminates_early_upon_client_process_death.cc': {},
    'test/sandbox/sandbox_uses_cgroup_namespace.cc': {'tester': 'test/sandbox/sandbox_uses_cgroup_namespace_tester.cc'},
    'test/sandbox/sandbox_uses_cgroups.cc': {'tester': 'test/sandbox/sandbox_uses_cgroups_tester.cc'},
    'test/sandbox/sandbox_uses_ipc_namespace.cc': {'tester': 'test/sandbox/sandbox_uses_ipc_namespace_tester.cc'},
    'test/sandbox/sandbox_uses_net_namespace.cc': {'tester': 'test/sandbox/sandbox_uses_net_namespace_tester.cc'},
    'test/sandbox/sandbox_uses_pid_namespace.cc': {'tester': 'test/sandbox/sandbox_uses_pid_namespace_tester.cc'},
    'test/sandbox/sandbox_uses_time_namespace.cc': {'tester': 'test/sandbox/sandbox_uses_time_namespace_tester.cc'},
    'test/sandbox/sandbox_uses_user_namespace.cc': {'tester': 'test/sandbox/sandbox_uses_user_namespace_tester.cc'},
    'test/sandbox/sandbox_uses_uts_namespace.cc': {'tester': 'test/sandbox/sandbox_uses_uts_namespace_tester.cc'},
    'test/sandbox/si.cc': {},
    'test/sandbox/simple.cc': {},
    'test/sandbox/tracee_cgroup_limits.cc': {'tester': 'test/sandbox/tracee_cgroup_limits_tester.cc'},
    'test/sandbox/tracee_cpu_time_limit.cc': {'tester': 'test/sandbox/tracee_cpu_time_limit_tester.cc'},
    'test/sandbox/tracee_hacks_limits_by_disabling_cgroup_controllers.cc': {'tester': 'test/sandbox/tracee_hacks_limits_by_disabling_cgroup_controllers_tester.cc'},
    'test/sandbox/tracee_is_killed_upon_supervisor_process_death.cc': {},
    'test/sandbox/tracee_memory_usage.cc': {'tester': 'test/sandbox/tracee_memory_usage_tester.cc'},
    'test/sandbox/tracee_no_new_privs.cc': {'tester': 'test/sandbox/tracee_no_new_privs_tester.cc'},
    'test/sandbox/tracee_prlimits.cc': {'tester': 'test/sandbox/tracee_prlimits_tester.cc', 'priority': 20 },
    'test/sandbox/tracee_seccomp.cc': {},
    'test/sandbox/tracee_time_limit.cc': {},
    'test/sandbox/tracee_times.cc': {'tester': 'test/sandbox/tracee_times_tester.cc'},
    'test/serialize.cc': {},
    'test/sha.cc': {},
    'test/shared_function.cc': {},
    'test/shared_memory_segment.cc': {},
    'test/signal_blocking.cc': {},
    'test/signal_handling.cc': {},
    'test/sim/judge/language_suite/bash.cc': {},
    'test/sim/judge/language_suite/c_clang.cc': {'priority': 10},
    'test/sim/judge/language_suite/c_gcc.cc': {'priority': 10},
    'test/sim/judge/language_suite/cpp_clang.cc': {'priority': 10},
    'test/sim/judge/language_suite/cpp_gcc.cc': {'priority': 10},
    'test/sim/judge/language_suite/pascal.cc': {'priority': 10},
    'test/sim/judge/language_suite/python.cc': {},
    'test/sim/judge/language_suite/rust.cc': {'priority': 10},
    'test/sim/judge/test_on_interactive_test.cc': {'priority': 10},
    'test/sim/judge/test_on_test.cc': {'priority': 10},
    'test/sim/problem_package.cc': {},
    'test/simfile.cc': {},
    'test/simple_parser.cc': {},
    'test/slice.cc': {},
    'test/sock_stream_ext.cc': {},
    'test/spawner.cc': {},
    'test/static_cstring_buff.cc': {},
    'test/string_compare.cc': {},
    'test/string_traits.cc': {},
    'test/string_transform.cc': {},
    'test/string_view.cc': {},
    'test/strongly_typed_function.cc': {},
    'test/temporary_directory.cc': {'dependencies': [gtest_main_dep, gmock_dep]},
    'test/temporary_file.cc': {'dependencies': [gtest_main_dep, gmock_dep]},
    'test/throw_assert.cc': {},
    'test/time.cc': {},
    'test/time_format_conversions.cc': {},
    'test/to_arg_seq.cc': {},
    'test/to_string.cc': {},
    'test/unlinked_temporary_file.cc': {},
    'test/utilities.cc': {},
    'test/working_directory.cc': {},
    'test/write_as_bytes.cc': {},
    'test/write_exact.cc': {},
}

foreach test_src, args : tests
    test_deps = ['tester' in args ? gtest_dep : gtest_main_dep]
    test_kwargs = {}
    foreach key, value : args
        if key == 'dependencies'
            test_deps = value
        elif key == 'tester'
            test_kwargs += {
                'args': [
                    executable(value.underscorify(),
                        implicit_include_directories : false,
                        sources : value,
                        build_by_default : false,
                        dependencies : simlib_dep,
                    ),
                ],
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
                test_deps,
            ],
            build_by_default : false,
        ),
        timeout : 300,
        kwargs : test_kwargs,
    )
endforeach

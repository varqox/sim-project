project('simlib',
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
        '-Wshadow=local',
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
    static_kwargs = {'static': true}
    add_project_link_arguments(['-static'], language : ['c', 'cpp'])
endif

dump_file_as_c_array = generator(find_program('dump_file_as_c_array.py'),
    output : ['@BASENAME@_dump.c'],
    arguments : ['@INPUT@', '@EXTRA_ARGS@'],
    capture : true,
)

libseccomp_dep = dependency('libseccomp', kwargs : static_kwargs)
libzip_dep = dependency('libzip', version : '>=1.3.0', kwargs : static_kwargs)
rt_dep = cpp.find_library('rt', kwargs : static_kwargs)

threads_dep = dependency('threads', kwargs : static_kwargs)
if get_option('static')
    threads_dep = declare_dependency(
        compile_args : '-pthread',
        link_args : ['-Wl,--whole-archive', '-lpthread', '-Wl,--no-whole-archive']
    )
endif

simlib_dependencies = [
    libseccomp_dep,
    libzip_dep,
    rt_dep,
    threads_dep,
    declare_dependency(
        include_directories : include_directories('3rdparty', is_system : true),
    ),
]

simlib_incdir = include_directories('include', is_system : false)
simlib_libsources = files([
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
    'src/path.cc',
    'src/proc_stat_file_contents.cc',
    'src/proc_status_file.cc',
    'src/process.cc',
    'src/random.cc',
    'src/sandbox.cc',
    'src/sha.cc',
    'src/sim/checker.cc',
    'src/sim/compile.cc',
    'src/sim/conver.cc',
    'src/sim/judge_worker.cc',
    'src/sim/problem_package.cc',
    'src/sim/simfile.cc',
    'src/spawner.cc',
    'src/string_compare.cc',
    'src/temporary_directory.cc',
    'src/temporary_file.cc',
    'src/time.cc',
    'src/unlinked_temporary_file.cc',
    'src/working_directory.cc',
])

simlib = library('simlib', sources : [
        simlib_libsources,
        dump_file_as_c_array.process(
            'src/sim/default_checker.c',
            extra_args : 'default_checker_c'
        ),
    ],
    include_directories : simlib_incdir,
    dependencies : simlib_dependencies,
    install : true,
)

simlib_dep = declare_dependency(
    include_directories : simlib_incdir,
    link_with : simlib,
    dependencies : simlib_dependencies,
)

alias_target('base', simlib)
run_target('format', command : [find_program('format.py'), meson.current_source_dir()])
run_target('tidy', command : [find_program('tidy')], depends : simlib)

#################################### Tests ####################################

gtest_main_dep = dependency('gtest', main : true, required : false, method : 'system')
gmock_dep = dependency('gmock', main : false, required : false, method : 'system')

if not gtest_main_dep.found() or not gmock_dep.found()
    gtest = subproject('gtest')
endif
if not gtest_main_dep.found()
    gtest_main_dep = declare_dependency(
        include_directories : gtest.get_variable('gtest_incdir'),
        link_with : library('gtest_main',
            dependencies : gtest.get_variable('gtest_main_dep')
        ),
        dependencies : threads_dep,
    )
endif
if not gmock_dep.found()
    gmock_dep = declare_dependency(
        include_directories : gtest.get_variable('gmock_incdir'),
        link_with : library('gmock',
            dependencies : gtest.get_variable('gmock_dep')
        ),
        dependencies : threads_dep,
    )
endif

tests = [
    ['test/argv_parser.cc', [], {}],
    ['test/call_in_destructor.cc', [], {}],
    ['test/concat.cc', [], {}],
    ['test/concat_common.cc', [], {}],
    ['test/concat_tostr.cc', [], {}],
    ['test/config_file.cc', [], {'priority': 10}],
    ['test/conver.cc', [], {'priority': 10}],
    ['test/ctype.cc', [], {}],
    ['test/debug.cc', [], {}],
    ['test/defer.cc', [], {}],
    ['test/directory.cc', [], {}],
    ['test/enum_val.cc', [], {}],
    ['test/err_defer.cc', [], {}],
    ['test/event_queue.cc', [], {}],
    ['test/fd_pread_buff.cc', [], {}],
    ['test/file_contents.cc', [], {}],
    ['test/file_descriptor.cc', [], {}],
    ['test/file_info.cc', [], {}],
    ['test/file_manip.cc', [], {}],
    ['test/file_path.cc', [], {}],
    ['test/http/response.cc', [], {}],
    ['test/http/url_dispatcher.cc', [], {}],
    ['test/humanize.cc', [], {}],
    ['test/inotify.cc', [], {}],
    ['test/inplace_array.cc', [], {}],
    ['test/inplace_buff.cc', [], {}],
    ['test/json_str/json_str.cc', [], {}],
    ['test/libzip.cc', [], {}],
    ['test/logger.cc', [], {}],
    ['test/member_comparator.cc', [], {}],
    ['test/memory.cc', [], {}],
    ['test/mysql/mysql.cc', [], {}],
    ['test/opened_temporary_file.cc', [gmock_dep], {}],
    ['test/path.cc', [], {}],
    ['test/proc_stat_file_contents.cc', [], {}],
    ['test/proc_status_file.cc', [], {}],
    ['test/process.cc', [], {}],
    ['test/random.cc', [], {}],
    ['test/ranges.cc', [], {}],
    ['test/request_uri_parser.cc', [], {}],
    ['test/sandbox.cc', [], {'priority': 8}],
    ['test/sha.cc', [], {}],
    ['test/shared_function.cc', [], {}],
    ['test/shared_memory_segment.cc', [], {}],
    ['test/signal_blocking.cc', [], {}],
    ['test/signal_handling.cc', [], {}],
    ['test/sim/problem_package.cc', [], {}],
    ['test/simfile.cc', [], {}],
    ['test/simple_parser.cc', [], {}],
    ['test/spawner.cc', [], {}],
    ['test/string_compare.cc', [], {}],
    ['test/string_traits.cc', [], {}],
    ['test/string_transform.cc', [], {}],
    ['test/string_view.cc', [], {}],
    ['test/strongly_typed_function.cc', [], {}],
    ['test/temporary_directory.cc', [gmock_dep], {}],
    ['test/temporary_file.cc', [gmock_dep], {}],
    ['test/time.cc', [], {}],
    ['test/to_string.cc', [], {}],
    ['test/unlinked_temporary_file.cc', [], {}],
    ['test/utilities.cc', [], {}],
    ['test/working_directory.cc', [], {}],
]

foreach test : tests
    name = test[0].underscorify()
    exe = executable(name, sources : test[0], dependencies : [
        gtest_main_dep,
        simlib_dep,
        test[1],
    ], build_by_default : false)
    test(name, exe, timeout : 300, kwargs : test[2], workdir : meson.current_source_dir())
endforeach

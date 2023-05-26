project('simlib',
    ['c', 'cpp'],
    license: 'MIT',
    meson_version : '>=0.53',
    default_options : meson.get_cross_property('project_configuration', [
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

alias_target('base', simlib)
run_target('format', command : [find_program('format.py'), meson.current_source_dir()])
run_target('tidy', command : [find_program('tidy')], depends : simlib)

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
    'test/call_in_destructor.cc': {},
    'test/concat.cc': {},
    'test/concat_common.cc': {},
    'test/concat_tostr.cc': {},
    'test/config_file.cc': {'priority': 10},
    'test/constructor_traits.cc': {},
    'test/conver.cc': {'priority': 10},
    'test/ctype.cc': {},
    'test/debug.cc': {},
    'test/defer.cc': {},
    'test/directory.cc': {},
    'test/enum_val.cc': {},
    'test/err_defer.cc': {},
    'test/event_queue.cc': {},
    'test/fd_pread_buff.cc': {},
    'test/file_contents.cc': {},
    'test/file_descriptor.cc': {},
    'test/file_info.cc': {},
    'test/file_manip.cc': {},
    'test/file_path.cc': {},
    'test/http/response.cc': {},
    'test/http/url_dispatcher.cc': {},
    'test/humanize.cc': {},
    'test/inotify.cc': {},
    'test/inplace_array.cc': {},
    'test/inplace_buff.cc': {},
    'test/json_str/json_str.cc': {},
    'test/libzip.cc': {},
    'test/logger.cc': {},
    'test/macros.cc': {},
    'test/member_comparator.cc': {},
    'test/memory.cc': {},
    'test/mysql/mysql.cc': {},
    'test/noexcept_concat.cc': {},
    'test/opened_temporary_file.cc': {'dependencies': [gmock_dep]},
    'test/path.cc': {},
    'test/proc_stat_file_contents.cc': {},
    'test/proc_status_file.cc': {},
    'test/process.cc': {},
    'test/random.cc': {},
    'test/ranges.cc': {},
    'test/request_uri_parser.cc': {},
    'test/result.cc': {},
    'test/sandbox.cc': {'priority': 8},
    'test/sha.cc': {},
    'test/shared_function.cc': {},
    'test/shared_memory_segment.cc': {},
    'test/signal_blocking.cc': {},
    'test/signal_handling.cc': {},
    'test/sim/problem_package.cc': {},
    'test/simfile.cc': {},
    'test/simple_parser.cc': {},
    'test/spawner.cc': {},
    'test/string_compare.cc': {},
    'test/string_traits.cc': {},
    'test/string_transform.cc': {},
    'test/string_view.cc': {},
    'test/strongly_typed_function.cc': {},
    'test/temporary_directory.cc': {'dependencies': [gmock_dep]},
    'test/temporary_file.cc': {'dependencies': [gmock_dep]},
    'test/time.cc': {},
    'test/to_string.cc': {},
    'test/unlinked_temporary_file.cc': {},
    'test/utilities.cc': {},
    'test/working_directory.cc': {},
}

foreach test_src, args : tests
    test_deps = [gtest_main_dep, args.get('dependencies', [])]
    test_kwargs = {}
    foreach key, value : args
        if key != 'dependencies'
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

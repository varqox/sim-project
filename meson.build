project('sim-project',
    [],
    license: 'MIT',
    meson_version : '>=0.58',
    default_options : meson.get_external_property('project_configuration', [])
)

_build = get_option('build')
_install = get_option('install')
build_simlib = _build.contains('all') or _build.contains('simlib') or _install.contains('simlib')
build_sim = _build.contains('all') or _build.contains('sim') or _install.contains('sim')
build_sip = _build.contains('all') or _build.contains('sip') or _install.contains('sip')

if build_simlib
    simlib_proj = subproject(
        'simlib',
        default_options : [
            'install=' + (_install.contains('simlib') or _install.contains('sim') or _install.contains('sip') ? 'true' : 'false'),
        ],
    )
endif

if build_sim
    sim_proj = subproject(
        'sim',
        default_options : [
            'install=' + (_install.contains('sim') ? 'true' : 'false'),
        ],
    )
endif

if build_sip
    sip_proj = subproject(
        'sip',
        default_options : [
            'install=' + (_install.contains('sip') ? 'true' : 'false'),
        ],
    )
endif

run_target('format', command : [find_program('format.py')])
run_target('tidy', command : [find_program('tidy')])

#!/usr/bin/env python3

from subprojects.simlib.format import filter_subdirs, format_sources, simlib_sources
import sys

def sim_sources(srcdir):
    return list(filter_subdirs(srcdir, [
        'include/',
        'src/',
        'test/',
    ], ['c', 'cc', 'h', 'hh'], [
        'src/web_server/static/.*',
        'test/sim/cpp_syntax_highlighter_test_cases/.*',
    ]))

if __name__ == '__main__':
    sources = sim_sources(sys.argv[1]) + simlib_sources(sys.argv[1] + '/subprojects/simlib')
    print('files to format:', len(sources))
    format_sources(sources)

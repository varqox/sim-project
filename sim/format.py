#!/usr/bin/env python3

from subprojects.simlib.format import filter_subdirs, format_sources, simlib_sources
import sys

def sim_sources(srcdir):
    return list(filter_subdirs(srcdir, [
        'src/',
        'src/include/sim/',
        'src/include/sim/http/',
        'src/job_server/',
        'src/job_server/job_handlers/',
        'src/lib/',
        'src/sim_merger/',
        'src/web_interface/',
        'test/'
    ], ['c', 'cc', 'h', 'hh'], [
    ]))

if __name__ == '__main__':
    sources = sim_sources(sys.argv[1]) + simlib_sources(sys.argv[1] + '/subprojects/simlib')
    print('files to format:', len(sources))
    format_sources(sources)

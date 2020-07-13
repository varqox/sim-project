#!/usr/bin/env python3

from subprojects.simlib.format import filter_subdirs, format_sources, simlib_sources
import sys

def sip_sources(srcdir):
    return list(filter_subdirs(srcdir, [
        'src/',
        'templates/',
    ], ['c', 'cc', 'h', 'hh'], [
        'src/git_commit.hh',
        'src/proot_dump.c',
    ]))

if __name__ == '__main__':
    sources = sip_sources(sys.argv[1]) + simlib_sources(sys.argv[1] + '/subprojects/simlib')
    print('files to format:', len(sources))
    format_sources(sources)

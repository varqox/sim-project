#!/usr/bin/env python3

from subprojects.simlib.format import filter_subdirs, format_sources, simlib_sources
import sys
import re

def sim_sources(srcdir):
    return list(filter_subdirs(srcdir, [
        'include/',
        'src/',
        'test/',
    ], ['c', 'cc', 'h', 'hh'], [
        'src/web_server/static/.*',
        'test/sim/cpp_syntax_highlighter_test_cases/.*',
    ]))

def update_clang_format_file(srcdir):
    with open(srcdir + '/subprojects/simlib/.clang-format', 'rb') as f:
        content = f.read()
    (content, count) = re.subn(rb'^(WhitespaceSensitiveMacros:)$', rb"\1\n  - VALIDATE", content, 0, re.MULTILINE)
    assert count == 1
    with open(srcdir + '/.clang-format', 'wb') as f:
        f.write(content)

if __name__ == '__main__':
    update_clang_format_file(sys.argv[1])
    sources = sim_sources(sys.argv[1]) + simlib_sources(sys.argv[1] + '/subprojects/simlib')
    print('files to format:', len(sources))
    format_sources(sources)

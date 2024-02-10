#!/usr/bin/env python3

from subprojects.simlib.format import filter_subdirs, Source, simlib_sources, format_sources
import os
import sys
import re

def sim_sources(srcdir):
    return [
        Source(path,
            os.path.join(os.path.dirname(__file__), '.clang-format'),
            [__file__],
            [
                # Fix includes of form "simlib/*", "gtest/*" "gmock/*" (thanks clangd...) to <...>
                ["sed", r's@^#include "\(\(sim/\|simlib/\|gtest/\|gmock/\).*\)"$@#include <\1>@', "-i", path]
            ]
        )
        for path in filter_subdirs(
            srcdir,
            [
                'include/',
                'src/',
                'test/',
            ],
            ['c', 'cc', 'h', 'hh'],
            [
                'src/web_server/static/.*',
                'test/sim/cpp_syntax_highlighter_test_cases/.*',
            ]
        )
    ]

def update_clang_format_file(srcdir):
    with open(srcdir + '/subprojects/simlib/.clang-format', 'rb') as f:
        content = f.read()
    (content, count) = re.subn(rb'^(WhitespaceSensitiveMacros:)$', rb"\1\n  - VALIDATE", content, 0, re.MULTILINE)
    assert count == 1
    # Check if something changed, not to trigger reformatting of everything due to .clang-format having been updated
    with open(srcdir + '/.clang-format', 'rb') as f:
        old_content = f.read()
    if content != old_content:
        with open(srcdir + '/.clang-format', 'wb') as f:
            f.write(content)

if __name__ == '__main__':
    update_clang_format_file(sys.argv[1])
    format_sources(sim_sources(sys.argv[1]) + simlib_sources(sys.argv[1] + '/subprojects/simlib'))

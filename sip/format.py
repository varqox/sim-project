#!/usr/bin/env python3

from subprojects.simlib.format import filter_subdirs, Source, simlib_sources, format_sources
import os
import sys

def sip_sources(srcdir):
    return [
        Source(path,
            os.path.join(os.path.dirname(__file__), '.clang-format'),
            [__file__],
            [
                # Fix includes of form "simlib/*", "gtest/*" "gmock/*" (thanks clangd...) to <...>
                ["sed", r's@^#include "\(\(simlib/\|gtest/\|gmock/\).*\)"$@#include <\1>@', "-i", path]
            ]
        )
        for path in filter_subdirs(
            srcdir,
            [
                'src/',
                'templates/',
                'test/',
            ],
            ['c', 'cc', 'h', 'hh'],
            [
                'src/git_commit.hh',
                'src/proot_dump.c',
                'test/sip_test_cases/.*'
            ]
        )
    ]

if __name__ == '__main__':
    format_sources(sip_sources(sys.argv[1]) + simlib_sources(sys.argv[1] + '/subprojects/simlib'))

#!/usr/bin/env python3

from concurrent.futures import ThreadPoolExecutor
import pathlib
import re
import subprocess
import sys

def filter_subdirs(dir, subdirs, allowed_extensions, denied_suffixes):
    dir = pathlib.Path(dir)
    allowed_extensions = ['.' + ext for ext in allowed_extensions]
    def qualifies(path):
        for ext in allowed_extensions:
            if path.endswith(ext):
                for dsuf in denied_suffixes:
                    if re.search(dsuf + '$', path):
                        return False
                return True
        return False

    return filter(qualifies, [str(f) for subdir in subdirs for f in (dir / subdir).glob('**/*')])

def format_sources(sources):
    with ThreadPoolExecutor() as e:
        def run_clang_format(f):
            print('format ' + f)
            return subprocess.check_call(['clang-format', '-style=file', '-i', f])

        futures = [e.submit(run_clang_format, f) for f in sources]
        [fut.result() for fut in futures]

def simlib_sources(srcdir):
    return list(filter_subdirs(srcdir, [
        'include/',
        'src/',
        'test/',
    ], ['c', 'cc', 'h', 'hh'], [
        'src/sim/default_checker_dump.c',
        'test/conver_test_cases/.*',
        'test/sandbox_test_cases/.*',
    ]))

if __name__ == '__main__':
    sources = simlib_sources(sys.argv[1])
    print('files to format:', len(sources))
    # Fix includes of form "simlib/*" (thanks clangd...) to <simlib/*>
    subprocess.run(["sed", r's@^#include "\(simlib/.*\)"$@#include <\1>@', "-i"] + [s for s in sources])
    format_sources(sources)

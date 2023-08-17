#!/usr/bin/env python3

from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import json
import os
import re
import subprocess
import sys
import time

def filter_subdirs(dir, subdirs, allowed_extensions, denied_suffixes):
    dir = Path(os.path.abspath(dir))
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

class Source:
    def __init__(self, path, dependency_files, pre_format_commands = []):
        self.path = path
        self.dependency_files = dependency_files
        self.pre_format_commands = pre_format_commands

def simlib_sources(src_dir):
    # Fix includes of form "simlib/*" and "gmock/*" (thanks clangd...) to <...>
    return [Source(path, [__file__], [["sed", r's@^#include "\(\(simlib/\|gtest\|gmock/\).*\)"$@#include <\1>@', "-i", path]]) for path in filter_subdirs(src_dir, [
        'include/',
        'src/',
        'test/',
    ], ['c', 'cc', 'h', 'hh'], [
        'src/sim/default_checker_dump.c',
        'test/conver_test_cases/.*',
        'test/sandbox_test_cases/.*',
    ])]

def format_sources(sources, cache_dir = Path(os.getenv('MESON_SOURCE_ROOT', '.')) / '.cache'):
    clang_format_path = subprocess.check_output(['which', 'clang-format']).strip()
    newest_dependency_files_mtime = max(map(os.path.getmtime, [clang_format_path, __file__]))
    # Make source paths absolute
    for source in sources:
        source.path = os.path.abspath(source.path)
    # Cache contains pairs: path => last reformat time
    cache = {}
    try:
        with open(Path(cache_dir) / 'format.json') as cache_file:
            cache = json.load(cache_file)
    except FileNotFoundError:
        pass

    def needs_reformatting(source):
        last_reformat = cache.get(source.path)
        if last_reformat is None:
            return True
        if last_reformat < os.path.getmtime(source.path):
            return True
        if last_reformat < max(newest_dependency_files_mtime, *map(os.path.getmtime, source.dependency_files)):
            return True
        return False

    # Trim cache to only contain the current sources i.e. remove sources that disappeared
    cache = {source.path: cache[source.path] for source in sources if source.path in cache}

    sources_to_format = {source for source in sources if needs_reformatting(source)}
    print(len(sources_to_format), 'files to format.')

    with ThreadPoolExecutor() as executor:
        def run_clang_format(source):
            print('format ' + source.path)
            for command in source.pre_format_commands:
                subprocess.check_call(command)
            subprocess.check_call(['clang-format', '-style=file', '-i', source.path])
            cache[source.path] = time.time() # this is thread safe

        futures = [executor.submit(run_clang_format, source) for source in sources_to_format]
        [fut.result() for fut in futures]

    # Save cache
    try:
        os.mkdir(cache_dir)
    except FileExistsError:
        pass
    with open(Path(cache_dir) / 'format.json', 'w') as cache_file:
        json.dump(cache, cache_file)

if __name__ == '__main__':
    format_sources(simlib_sources(sys.argv[1]))

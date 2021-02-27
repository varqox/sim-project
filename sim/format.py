#!/usr/bin/env python3

from subprojects.simlib.format import filter_subdirs, format_sources, simlib_sources
import sys

def sim_sources(srcdir):
    return list(filter_subdirs(srcdir, [
        'include/',
        'include/sim/',
        'include/sim/contest_files/',
        'include/sim/contest_problems/',
        'include/sim/contest_questions_and_news/',
        'include/sim/contest_rounds/',
        'include/sim/contest_users/',
        'include/sim/contests/',
        'include/sim/jobs/',
        'include/sim/mysql/',
        'include/sim/problems/',
        'include/sim/sessions/',
        'include/sim/sql_fields/',
        'include/sim/submissions/',
        'include/sim/users/',
        'include/sim/web_api/',
        'src/',
        'src/job_server/',
        'src/job_server/job_handlers/',
        'src/sim/',
        'src/sim/contest_files/',
        'src/sim/contests/',
        'src/sim/jobs/',
        'src/sim/mysql/',
        'src/sim/problems/',
        'src/sim/submissions/',
        'src/sim_merger/',
        'src/web_server/',
        'src/web_server/http/',
        'src/web_server/old/',
        'src/web_server/server/',
        'src/web_server/web_worker/',
        'test/',
        'test/sim/',
        'test/sim/cpp_syntax_highlighter_test_cases/',
        'test/sim/jobs/',
    ], ['c', 'cc', 'h', 'hh'], [
    ]))

if __name__ == '__main__':
    sources = sim_sources(sys.argv[1]) + simlib_sources(sys.argv[1] + '/subprojects/simlib')
    print('files to format:', len(sources))
    format_sources(sources)

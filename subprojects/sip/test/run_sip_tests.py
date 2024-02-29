#!/usr/bin/env python3

from concurrent.futures import ThreadPoolExecutor
import pathlib
import subprocess
import sys
import os

if len(sys.argv) < 2:
    sys.exit('Usage: ' + sys.argv[0] + ' <sip_command> [max_workers]')

sip_binary = sys.argv[1]
test_cases_dir = os.path.dirname(os.path.realpath(__file__)) + '/sip_test_cases/tests'

def run_tests(tests_names):
    max_workers = None
    if len(sys.argv) > 2:
        max_workers = int(sys.argv[2])
    with ThreadPoolExecutor(max_workers=max_workers) as e:
        def run_test_case(test_name):
            print('\033[1m>>>    ' + test_name + '    <<<\033[m')
            return subprocess.check_call([test_cases_dir + '/' + test_name + '/run.sh', sip_binary])

        futures = [e.submit(run_test_case, t) for t in tests_names]
        [fut.result() for fut in futures]

run_tests(os.listdir(test_cases_dir))

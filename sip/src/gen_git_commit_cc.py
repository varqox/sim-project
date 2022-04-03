#!/usr/bin/env python3

import subprocess
import sys

commit = subprocess.check_output(['git', '--git-dir=' + sys.argv[1], 'rev-parse', 'HEAD']).strip().decode('ascii')
print(f'extern constexpr auto COMMIT = "{commit}";')

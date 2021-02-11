#!/usr/bin/env python3

import subprocess

commit = subprocess.check_output(['git', 'rev-parse', 'HEAD']).strip().decode('ascii')
print(f'extern constexpr auto COMMIT = "{commit}";')

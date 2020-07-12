#!/usr/bin/env python3

import subprocess

commit = subprocess.check_output(['git', 'rev-parse', 'HEAD']).strip().decode('ascii')
print(f'#pragma once\nconstexpr inline auto COMMIT = "{commit}";')

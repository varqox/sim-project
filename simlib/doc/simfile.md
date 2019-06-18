# Simfile - Sim package configuration file
Simfile is a ConfigFile file, so the syntax is the same as in the
ConfigFile

## Example:
```sh
name: Simple Package                     # Problem name
label: sim                               # Problem label (usually a
                                         #   shortened name)
interactive: false                       # Whether the problem is
                                         #   interactive or not
                                         #   (optional - default false)
statement: doc/sim.pdf                   # Path to statement file
checker: check/checker.cpp               # Path to checker source file
                                         #   (optional if the problem
                                         #     is not interactive),
                                         #   if not set, the default
                                         #   checker will be used
solutions: [prog/sim.cpp, prog/sim1.cpp] # Paths to solutions' source files.
                                         #   The first solution is the main
                                         #   solution
memory_limit: 64           # Global memory limit in MB (optional)
limits: [                  # Limits array
        # Group 0
        sim0a 1        # Format: <test name> <time limit> [memory limit]
        sim0b 1.01     # Time limit in seconds, memory limit in MB
        sim1ocen 2 32  # Individual memory limit is optional if the global
                       #   memory limit is set.
        sim2ocen 3     # Tests may appear in an arbitrary order

        # Group 1
        sim1a 1
        sim1b 1

        # Group 2
        sim2a 2
        sim2b 2

        # Group 3
        sim3a 3
        sim3b 3

        # Group 4
        sim4 5 32
]
scoring: [                 # Scoring of the tests groups (optional)
        0 0      # Format: <group id> <score>
        1 20     # Score is a signed integer value
        2 30     # Groups may appear in an arbitrary order
        3 25
        4 25
]
tests_files: [         # Tests' input and output files (optional)
                       # Format: <test name> <in file> <out file - present
                       #   only if the package is not interactive>
        sim0a in/sim0a.in out/sim0a.out
        sim0b in/sim0b.in out/sim0b.out
        sim1ocen in/sim1ocen.in out/sim1ocen.out
        sim2ocen in/sim2ocen.in out/sim2ocen.out
        sim1a in/sim1a.in out/sim1a.out
        sim1b in/sim1b.in out/sim1b.out
        sim2a in/sim2a.in out/sim2a.out
        sim2b in/sim2b.in out/sim2b.out
        sim3a in/sim3a.in out/sim3a.out
        sim3b in/sim3b.in out/sim3b.out
        sim4 in/sim4.in out/sim4.out
]
```

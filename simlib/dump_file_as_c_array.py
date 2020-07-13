#!/usr/bin/env python3
import sys

if len(sys.argv) != 3:
    sys.exit('Usage: ' + sys.argv[0] + ' <file> <c_array_name>')

file = sys.argv[1]
c_array_name = sys.argv[2]

with open(file, mode='rb') as f:
    contents = f.read()

def group_by_n(l, n):
    return [l[i:i + n] for i in range(0, len(l), n)]

lines_elems = group_by_n(['0x{:02x}'.format(x) for x in contents], 12)
dumped_lines = [', '.join(l) for l in lines_elems]
dumped_contents = ',\n\t'.join(dumped_lines)

print("""unsigned char {0}[] = {{
\t{1}
}};
unsigned int {0}_len = {2};""".format(c_array_name, dumped_contents, len(contents)))

#! /usr/bin/env python
"""A script to stitch together the generated text files in the correct order.
"""

import os
import sys

def main():
    if len(sys.argv) != 3:
        sys.stderr.write("usage: %s index.rst text-dir\n")
        return 2

    _, index, txt_dir = sys.argv

    for fb in iter_file_base(index):
        emit(fb, txt_dir)

    return 0

def iter_file_base(fn):
    f = open(fn)
    if sys.version_info[0] >= 3:
        have_line = iter(f).__next__
    else:
        have_line = iter(f).next

    while not have_line().startswith('.. toctree'):
        pass
    while have_line().strip().startswith(':'):
        pass

    yield os.path.splitext(os.path.basename(fn))[0]

    n = 0
    while True:
        line = have_line()
        if line.isspace():
            continue
        if line.startswith(".."):
            break
        n += 1
        yield line.strip()

    f.close()

    if n < 5:
        # maybe format changed?
        raise Exception("Not enough files found. Format change in index.rst?")

def emit(basename, txt_dir):
    f = open(os.path.join(txt_dir, basename + ".txt"))
    for line in f:
        line = line.replace("``", "'")
        sys.stdout.write(line)
    f.close()

    # some space between sections
    sys.stdout.write("\n\n")

    
if __name__ == '__main__':
    sys.exit(main())


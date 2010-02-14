#! /usr/bin/env python
"""A script to stitch together the generated text files in the correct order.
"""

import os
import sys

def main():
    if len(sys.argv) != 3:
        print >>sys.stderr, "usage: %s index.rst text-dir"
        return 2

    _, index, txt_dir = sys.argv

    for fb in iter_file_base(index):
        emit(fb, txt_dir)

    return 0

def iter_file_base(fn):
    have_line = iter(open(fn)).next

    while not have_line().startswith('.. toctree'):
        pass
    while have_line().strip().startswith(':'):
        pass

    yield os.path.splitext(os.path.basename(fn))[0]

    n = 0
    while 1:
        line = have_line()
        if line.isspace():
            continue
        if line.startswith(".."):
            break
        n += 1
        yield line.strip()

    if n < 5:
        # maybe format changed?
        raise Exception("Not enough files found. Format change in index.rst?")

def emit(basename, txt_dir):
    for line in open(os.path.join(txt_dir, basename + ".txt")):
        line = line.replace("``", "'")
        sys.stdout.write(line)

    # some space between sections
    print
    print

    
if __name__ == '__main__':
    sys.exit(main())


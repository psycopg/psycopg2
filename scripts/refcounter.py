#!/usr/bin/env python
"""Detect reference leaks after several unit test runs.

The script runs the unit test and counts the objects alive after the run. If
the object count differs between the last two runs, a report is printed and the
script exits with error 1.
"""

# Copyright (C) 2011 Daniele Varrazzo <daniele.varrazzo@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

import gc
import sys
import difflib
import unittest
from pprint import pprint
from collections import defaultdict

def main():
    opt = parse_args()

    import psycopg2.tests
    test = psycopg2.tests
    if opt.suite:
        test = getattr(test, opt.suite)

    sys.stdout.write("test suite %s\n" % test.__name__)

    for i in range(1, opt.nruns + 1):
        sys.stdout.write("test suite run %d of %d\n" % (i, opt.nruns))
        runner = unittest.TextTestRunner()
        runner.run(test.test_suite())
        dump(i, opt)

    f1 = open('debug-%02d.txt' % (opt.nruns - 1)).readlines()
    f2 = open('debug-%02d.txt' % (opt.nruns)).readlines()
    for line in difflib.unified_diff(f1, f2,
            "run %d" % (opt.nruns - 1), "run %d" % opt.nruns):
        sys.stdout.write(line)

    rv = f1 != f2 and 1 or 0

    if opt.objs:
        f1 = open('objs-%02d.txt' % (opt.nruns - 1)).readlines()
        f2 = open('objs-%02d.txt' % (opt.nruns)).readlines()
        for line in difflib.unified_diff(f1, f2,
                "run %d" % (opt.nruns - 1), "run %d" % opt.nruns):
            sys.stdout.write(line)

    return rv

def parse_args():
    import optparse

    parser = optparse.OptionParser(description=__doc__)
    parser.add_option('--nruns', type='int', metavar="N", default=3,
        help="number of test suite runs [default: %default]")
    parser.add_option('--suite', metavar="NAME",
        help="the test suite to run (e.g. 'test_cursor'). [default: all]")
    parser.add_option('--objs', metavar="TYPE",
        help="in case of leaks, print a report of object TYPE "
            "(support still incomplete)")

    opt, args = parser.parse_args()
    return opt


def dump(i, opt):
    gc.collect()
    objs = gc.get_objects()

    c = defaultdict(int)
    for o in objs:
        c[type(o)] += 1

    pprint(
        sorted(((v,str(k)) for k,v in c.items()), reverse=True),
        stream=open("debug-%02d.txt" % i, "w"))

    if opt.objs:
        co = []
        t = getattr(__builtins__, opt.objs)
        for o in objs:
            if type(o) is t:
                co.append(o)

        # TODO: very incomplete
        if t is dict:
            co.sort(key = lambda d: d.items())
        else:
            co.sort()

        pprint(co, stream=open("objs-%02d.txt" % i, "w"))


if __name__ == '__main__':
    sys.exit(main())


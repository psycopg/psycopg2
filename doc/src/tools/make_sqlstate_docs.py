#!/usr/bin/env python
"""Create the docs table of the sqlstate errors.
"""

from __future__ import print_function

import re
import sys
from collections import namedtuple

from psycopg2._psycopg import sqlstate_errors


def main():
    sqlclasses = {}
    clsfile = sys.argv[1]
    with open(clsfile) as f:
        for l in f:
            m = re.match(r'/\* Class (..) - (.+) \*/', l)
            if m is not None:
                sqlclasses[m.group(1)] = m.group(2)

    Line = namedtuple('Line', 'colstate colexc colbase sqlstate')

    lines = [Line('SQLSTATE', 'Exception', 'Base exception', None)]
    for k in sorted(sqlstate_errors):
        exc = sqlstate_errors[k]
        lines.append(Line(
            "``%s``" % k, "`!%s`" % exc.__name__,
            "`!%s`" % get_base_exception(exc).__name__, k))

    widths = [max(len(l[c]) for l in lines) for c in range(3)]
    h = Line(*(['=' * w for w in widths] + [None]))
    lines.insert(0, h)
    lines.insert(2, h)
    lines.append(h)

    h1 = '-' * (sum(widths) + len(widths) - 1)
    sqlclass = None
    for l in lines:
        cls = l.sqlstate[:2] if l.sqlstate else None
        if cls and cls != sqlclass:
            print("**Class %s**: %s" % (cls, sqlclasses[cls]))
            print(h1)
            sqlclass = cls

        print("%-*s %-*s %-*s" % (
            widths[0], l.colstate, widths[1], l.colexc, widths[2], l.colbase))


def get_base_exception(exc):
    for cls in exc.__mro__:
        if cls.__module__ == 'psycopg2':
            return cls


if __name__ == '__main__':
    sys.exit(main())

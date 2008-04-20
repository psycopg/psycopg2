#!/usr/bin/env python
# types_basic.py - tests for basic types conversions
#
# Copyright (C) 2004 Federico Di Gregorio  <fog@debian.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTIBILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.

try:
    import decimal
except:
    pass
import sys
import unittest

import psycopg2
import tests


class TypesBasicTests(unittest.TestCase):
    """Test presence of mandatory attributes and methods."""

    def setUp(self):
        self.conn = psycopg2.connect(tests.dsn)

    def execute(self, *args):
        curs = self.conn.cursor()
        curs.execute(*args)
        return curs.fetchone()[0]

    def testQuoting(self):
        s = "Quote'this\\! ''ok?''"
        self.failUnless(self.execute("SELECT %s AS foo", (s,)) == s,
                        "wrong quoting: " + s)

    def testUnicode(self):
        s = u"Quote'this\\! ''ok?''"
        self.failUnless(self.execute("SELECT %s AS foo", (s,)) == s,
                        "wrong unicode quoting: " + s)

    def testNumber(self):
        s = self.execute("SELECT %s AS foo", (1971,))
        self.failUnless(s == 1971, "wrong integer quoting: " + str(s))
        s = self.execute("SELECT %s AS foo", (1971L,))
        self.failUnless(s == 1971L, "wrong integer quoting: " + str(s))
        # Python 2.4 defaults to Decimal?
        if sys.version_info[0] >= 2 and sys.version_info[1] >= 4:
            s = self.execute("SELECT %s AS foo", (19.10,))
            self.failUnless(s - decimal.Decimal("19.10") == 0,
                            "wrong decimal quoting: " + str(s))
        else:
            s = self.execute("SELECT %s AS foo", (19.10,))
            self.failUnless(abs(s - 19.10) < 0.001,
                            "wrong float quoting: " + str(s))

    def testBinary(self):
        s = ''.join([chr(x) for x in range(256)])
        b = psycopg2.Binary(s)
        buf = self.execute("SELECT %s::bytea AS foo", (b,))
        self.failUnless(str(buf) == s, "wrong binary quoting")

    def testBinaryEmptyString(self):
        # test to make sure an empty Binary is converted to an empty string
        b = psycopg2.Binary('')
        self.assertEqual(str(b), "''")

    def testBinaryRoundTrip(self):
        # test to make sure buffers returned by psycopg2 are
        # understood by execute:
        s = ''.join([chr(x) for x in range(256)])
        buf = self.execute("SELECT %s::bytea AS foo", (psycopg2.Binary(s),))
        buf2 = self.execute("SELECT %s::bytea AS foo", (buf,))
        self.failUnless(str(buf2) == s, "wrong binary quoting")

    def testArray(self):
        s = self.execute("SELECT %s AS foo", ([[1,2],[3,4]],))
        self.failUnless(s == [[1,2],[3,4]], "wrong array quoting " + str(s))
        s = self.execute("SELECT %s AS foo", (['one', 'two', 'three'],))
        self.failUnless(s == ['one', 'two', 'three'],
                        "wrong array quoting " + str(s))


def test_suite():
    return unittest.TestLoader().loadTestsFromName(__name__)

if __name__ == "__main__":
    unittest.main()


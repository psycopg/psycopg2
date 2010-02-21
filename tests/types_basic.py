#!/usr/bin/env python
#
# types_basic.py - tests for basic types conversions
#
# Copyright (C) 2004-2010 Federico Di Gregorio  <fog@debian.org>
#
# psycopg2 is free software: you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# In addition, as a special exception, the copyright holders give
# permission to link this program with the OpenSSL library (or with
# modified versions of OpenSSL that use the same license as OpenSSL),
# and distribute linked combinations including the two.
#
# You must obey the GNU Lesser General Public License in all respects for
# all of the code used other than OpenSSL.
#
# psycopg2 is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
# License for more details.

try:
    import decimal
except:
    pass
import sys
import unittest

import psycopg2
import tests


class TypesBasicTests(unittest.TestCase):
    """Test that all type conversions are working."""

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
        if sys.version_info[0] < 2 or sys.version_info[1] < 4:
            s = self.execute("SELECT %s AS foo", (19.10,))
            self.failUnless(abs(s - 19.10) < 0.001,
                        "wrong float quoting: " + str(s))

    def testDecimal(self):
        if sys.version_info[0] >= 2 and sys.version_info[1] >= 4:
            s = self.execute("SELECT %s AS foo", (decimal.Decimal("19.10"),))
            self.failUnless(s - decimal.Decimal("19.10") == 0,
                            "wrong decimal quoting: " + str(s))
            s = self.execute("SELECT %s AS foo", (decimal.Decimal("NaN"),))
            self.failUnless(str(s) == "NaN", "wrong decimal quoting: " + str(s))
            self.failUnless(type(s) == decimal.Decimal, "wrong decimal conversion: " + repr(s))
            s = self.execute("SELECT %s AS foo", (decimal.Decimal("infinity"),))
            self.failUnless(str(s) == "NaN", "wrong decimal quoting: " + str(s))
            self.failUnless(type(s) == decimal.Decimal, "wrong decimal conversion: " + repr(s))
            s = self.execute("SELECT %s AS foo", (decimal.Decimal("-infinity"),))
            self.failUnless(str(s) == "NaN", "wrong decimal quoting: " + str(s))
            self.failUnless(type(s) == decimal.Decimal, "wrong decimal conversion: " + repr(s))

    def testFloat(self):
        s = self.execute("SELECT %s AS foo", (float("nan"),))
        self.failUnless(str(s) == "nan", "wrong float quoting: " + str(s))
        self.failUnless(type(s) == float, "wrong float conversion: " + repr(s))
        s = self.execute("SELECT %s AS foo", (float("inf"),))
        self.failUnless(str(s) == "inf", "wrong float quoting: " + str(s))      
        self.failUnless(type(s) == float, "wrong float conversion: " + repr(s))

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


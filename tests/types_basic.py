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

import psycopg
from unittest import TestCase, TestSuite, main


class TypesBasicTests(TestCase):
    """Test presence of mandatory attributes and methods."""

    def setUp(self):
        self.conn = psycopg.connect("dbname=test")

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
        s = self.execute("SELECT %s AS foo", (19.10,))
        self.failUnless(s == 19.10, "wrong float quoting: " + str(s))

    def testBinary(self):
        s = ''.join([chr(x) for x in range(256)])
        b = psycopg.Binary(s)
        self.failUnless(self.execute("SELECT %s::bytea AS foo", (b,)) == s,
                        "wrong binary quoting")

class TypesBasicSuite(TestSuite):
    """Build a suite of all tests."""

    def __init__(self):
        """Build a list of tests."""
        self.tests = [x for x in dir(TypesBasicTests) if x.startswith('test')]
        TestSuite.__init__(self, map(TestModule, self.tests))


if __name__ == "__main__":
    main()

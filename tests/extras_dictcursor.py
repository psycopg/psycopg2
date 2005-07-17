# extras_dictcursor - test if DictCursor extension class works
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

import psycopg2
import psycopg2.extras
from unittest import TestCase, TestSuite, main


class ExtrasDictCursorTests(TestCase):
    """Test if DictCursor extension class works."""

    def setUp(self):
        self.conn = psycopg2.connect("dbname=test")
        curs = self.conn.cursor()
        curs.execute("CREATE TABLE ExtrasDictCursorTests (foo text)")
    
    def testDictCursor(self):
        curs = self.conn.cursor(cursor_factory=psycopg2.extras.DictCursor)
        curs.execute("INSERT INTO ExtrasDictCursorTests VALUES ('bar')")
        curs.execute("SELECT * FROM ExtrasDictCursorTests")
        row = curs.fetchone()
        self.failUnless(row['foo'] == 'bar')
        self.failUnless(row[0] == 'bar')

class ExtrasDictCursorSuite(TestSuite):
    """Build a suite of all tests."""

    def __init__(self):
        """Build a list of tests."""
        self.tests = [x for x in dir(ExtrasDictCursorTests)
					  if x.startswith('test')]
        TestSuite.__init__(self, map(TestModule, self.tests))


if __name__ == "__main__":
    main()
